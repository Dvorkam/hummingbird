#include "style/compute/StyleEngine.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/ArenaAllocator.h"
#include "core/GraphicsTypes.h"
#include "core/dom/Element.h"
#include "core/dom/Node.h"
#include "core/utils/StringUtils.h"
#include "html/HtmlAttributeNames.h"
#include "style/compute/StyleDefaults.h"
#include "style/compute/StyleValueUtils.h"
#include "style/compute/Stylesheet.h"
#include "style/compute/apply/ApplyColorUtils.h"
#include "style/compute/apply/PropertyApplier.h"
#include "style/registry/CssPropertyRegistry.h"
#include "style/selector/SelectorMatcher.h"
#include "style/types/ComputedStyle.h"

namespace Hummingbird::Css {

namespace {

struct MatchedProperty {
    int specificity;
    size_t order;
    Value value;
    bool important;
};

// Cascade: importance beats specificity beats source order (T-CSS-IMPORTANT-1).
bool declaration_wins(bool important, int specificity, size_t order, const MatchedProperty& incumbent) {
    if (important != incumbent.important) return important;
    if (specificity != incumbent.specificity) return specificity > incumbent.specificity;
    return order > incumbent.order;
}

struct StyleResult {
    ComputedStyle style;
    StyleDefaults::StyleOverrides overrides;
};

struct PropertyHash {
    size_t operator()(Property property) const { return static_cast<size_t>(property); }
};

using PropertyMap = std::unordered_map<Property, MatchedProperty, PropertyHash>;
using CustomPropertyMap = std::unordered_map<std::string, MatchedProperty>;

struct MatchedDeclarations {
    PropertyMap properties;
    CustomPropertyMap custom_properties;
};

// A single (rule, selector) pair, tagged with its document-order sequence so
// candidates gathered from several buckets can be re-sorted into cascade order.
struct RuleCandidate {
    const Rule* rule;
    const Selector* selector;
    size_t sequence;
};

// Rules bucketed by the key (rightmost) compound selector's most specific simple
// part, so an element only tests selectors that could plausibly match it instead
// of the whole sheet (T-PERF-STYLE-1). Media-non-matching rules are excluded at
// build time, since a build is scoped to one MediaContext.
struct RuleIndex {
    std::unordered_map<std::string, std::vector<RuleCandidate>> by_id;
    std::unordered_map<std::string, std::vector<RuleCandidate>> by_class;
    std::unordered_map<std::string, std::vector<RuleCandidate>> by_tag;
    std::vector<RuleCandidate> universal;
};

RuleIndex build_rule_index(const Stylesheet& sheet, const MediaContext& media) {
    RuleIndex index;
    size_t sequence = 0;
    for (const auto& rule : sheet.rules) {
        if (rule.media && !media_condition_matches(*rule.media, media)) {
            continue;
        }
        for (const auto& selector : rule.selectors) {
            RuleCandidate candidate{&rule, &selector, sequence++};
            if (selector.parts.empty()) {
                continue;
            }
            const SelectorPart& key = selector.parts.back();
            if (!key.id.empty()) {
                index.by_id[key.id].push_back(candidate);
            } else if (!key.classes.empty()) {
                index.by_class[key.classes.front()].push_back(candidate);
            } else if (!key.tag.empty() && key.tag != "*") {
                index.by_tag[key.tag].push_back(candidate);
            } else {
                index.universal.push_back(candidate);
            }
        }
    }
    return index;
}

MatchedDeclarations collect_matched_properties(const RuleIndex& index, const DOM::Node* node) {
    MatchedDeclarations matched;

    const auto* element = dynamic_cast<const DOM::Element*>(node);
    if (!element) return matched;

    // Gather the candidate selectors whose key part could match this element:
    // its id bucket, one bucket per class, its tag bucket, and the universal set.
    std::vector<const RuleCandidate*> candidates;
    auto append_bucket = [&](const std::unordered_map<std::string, std::vector<RuleCandidate>>& buckets,
                             const std::string& key) {
        auto it = buckets.find(key);
        if (it == buckets.end()) return;
        for (const auto& candidate : it->second) {
            candidates.push_back(&candidate);
        }
    };

    if (const auto* id = element->find_attribute(Hummingbird::Html::AttributeNames::Id); id && !id->empty()) {
        append_bucket(index.by_id, *id);
    }
    if (const auto* classes = element->find_attribute(Hummingbird::Html::AttributeNames::Class);
        classes && !classes->empty()) {
        for (auto token : Core::Utils::split_ascii_whitespace(*classes)) {
            append_bucket(index.by_class, std::string(token));
        }
    }
    append_bucket(index.by_tag, std::string(element->get_tag_name()));
    for (const auto& candidate : index.universal) {
        candidates.push_back(&candidate);
    }

    // Restore cascade (document) order across the mixed buckets before applying.
    std::sort(candidates.begin(), candidates.end(),
              [](const RuleCandidate* a, const RuleCandidate* b) { return a->sequence < b->sequence; });

    size_t order = 0;
    for (const auto* candidate : candidates) {
        if (!matches_selector(node, *candidate->selector)) continue;
        int spec = candidate->selector->specificity();
        for (const auto& decl : candidate->rule->declarations) {
            if (decl.property == Property::Custom) {
                if (decl.custom_property.empty()) {
                    ++order;
                    continue;
                }
                auto it = matched.custom_properties.find(decl.custom_property);
                if (it == matched.custom_properties.end() ||
                    declaration_wins(decl.important, spec, order, it->second)) {
                    matched.custom_properties[decl.custom_property] = {spec, order, decl.value, decl.important};
                }
                ++order;
                continue;
            }
            auto it = matched.properties.find(decl.property);
            if (it == matched.properties.end() || declaration_wins(decl.important, spec, order, it->second)) {
                matched.properties[decl.property] = {spec, order, decl.value, decl.important};
            }
            ++order;
        }
    }

    return matched;
}

void apply_properties_to_style(const PropertyMap& properties, ComputedStyle& style,
                               StyleDefaults::StyleOverrides& overrides, bool& display_set, float parent_font_size,
                               const ComputedStyle* parent_style) {
    Apply::Context context{parent_font_size, parent_style, &display_set};
    display_set = false;
    for (const auto& entry : PropertyRegistry::entries()) {
        auto it = properties.find(entry.property);
        if (it == properties.end()) {
            continue;
        }
        // CSS-wide `inherit` (T-CSS-INHERIT-1): leave the field untouched and
        // its override flag unset so inherit_from_parent copies the parent's
        // computed value. This is exact for inherited properties (DDG's
        // `font-family: inherit`); non-inherited properties fall back to
        // their initial value, which is the supported slice for now.
        const Value& value = it->second.value;
        if (value.type == Value::Type::Identifier && value.ident == "inherit") {
            continue;
        }
        // var() substitution for any property (T-CSS-VAR-3): resolve the
        // custom property to its raw text and re-type it (length/color/
        // number) before the apply hook runs. An unresolvable var leaves the
        // property at its initial/inherited value, per spec.
        if (value.type == Value::Type::Identifier && value.ident.starts_with("var(")) {
            auto text = Apply::resolve_var_text(style, parent_style, value.ident);
            if (!text) {
                continue;
            }
            Value substituted = StyleValueUtils::parse_substituted_value(*text);
            if (substituted.type == Value::Type::Identifier && substituted.ident == "inherit") {
                continue;
            }
            Apply::apply_property(entry.property, substituted, style, overrides, context);
            continue;
        }
        Apply::apply_property(entry.property, value, style, overrides, context);
    }
}

void apply_custom_properties(const CustomPropertyMap& properties, ComputedStyle& style) {
    for (const auto& [name, property] : properties) {
        // `--x: inherit` keeps the parent's value (custom properties inherit
        // by default; the parent merge in compute_node fills the gap).
        if (property.value.ident == "inherit") {
            continue;
        }
        style.custom_properties[name] = property.value.ident;
    }
}

// Fills inherited text/font properties from the parent for any field the
// element did not set itself (tracked by `overrides`). Non-inherited box
// properties are intentionally NOT handled here: the element's computed style
// already holds its own values, so they need no per-field copy and adding a new
// non-inherited property requires no change to this function. Only the small,
// stable set of genuinely inherited CSS properties is enumerated below; a new
// *inherited* property must be added here (guarded by
// StyleEngineTest.InheritsAllInheritedProperties).
void inherit_from_parent(ComputedStyle& style, const ComputedStyle& parent,
                         const StyleDefaults::StyleOverrides& overrides) {
    if (!overrides.color) style.color = parent.color;
    if (!overrides.underline) style.underline = parent.underline;
    if (!overrides.underline_thickness) style.underline_thickness = parent.underline_thickness;
    if (!overrides.underline_offset) style.underline_offset = parent.underline_offset;
    if (!overrides.link_color) style.link_color = parent.link_color;
    if (!overrides.vlink_color) style.vlink_color = parent.vlink_color;
    if (!overrides.whitespace) style.whitespace = parent.whitespace;
    if (!overrides.font_monospace) style.font_monospace = parent.font_monospace;
    if (!overrides.weight) style.weight = parent.weight;
    if (!overrides.style) style.style = parent.style;
    if (!overrides.font_size) style.font_size = parent.font_size;
    if (!overrides.font_face) style.font_face = parent.font_face;
    if (!overrides.text_align) style.text_align = parent.text_align;
    if (!overrides.text_transform) style.text_transform = parent.text_transform;
    if (!overrides.cursor) style.cursor = parent.cursor;
    if (!overrides.letter_spacing) style.letter_spacing = parent.letter_spacing;
    if (!overrides.text_indent) style.text_indent = parent.text_indent;
    if (!overrides.word_wrap) style.word_wrap = parent.word_wrap;
    if (!overrides.line_height) style.line_height = parent.line_height;
    if (!overrides.list_style_type) style.list_style_type = parent.list_style_type;
    if (!overrides.list_style_position) style.list_style_position = parent.list_style_position;
    if (!overrides.visibility) style.visibility = parent.visibility;
    if (!overrides.pointer_events) style.pointer_events = parent.pointer_events;
    if (!overrides.text_shadow) style.text_shadow = parent.text_shadow;
}

// Returns a computed style based on matching rules and parent style (for inheritance in the future).
StyleResult build_style_for(const RuleIndex& index, const DOM::Node* node, const ComputedStyle* parent_style) {
    StyleResult result{default_computed_style(), {}};
    ComputedStyle& style = result.style;
    MatchedDeclarations matched = collect_matched_properties(index, node);
    bool display_set = matched.properties.find(Property::Display) != matched.properties.end();

    // Minimal UA defaults for basic HTML readability.
    if (const auto* element = dynamic_cast<const DOM::Element*>(node)) {
        StyleDefaults::apply_user_agent_defaults(*element, style, result.overrides, display_set, parent_style);
        StyleDefaults::apply_legacy_attributes(*element, style, result.overrides);
    }

    apply_custom_properties(matched.custom_properties, style);

    float parent_font_size = parent_style ? parent_style->font_size : style.font_size;
    apply_properties_to_style(matched.properties, style, result.overrides, display_set, parent_font_size, parent_style);

    return result;
}

void compute_node(const RuleIndex& index, DOM::Node* node, const ComputedStyle* parent_style,
                  const FontFaceRegistry* fonts) {
    StyleResult own = build_style_for(index, node, parent_style);
    // Start from the element's own computed style: every non-inherited (box)
    // property is already correct by construction, so no per-field copy list is
    // needed. Only inherited properties fall back to the parent. (Text nodes
    // have no overrides, so they inherit everything.)
    ComputedStyle style = std::move(own.style);

    if (parent_style) {
        inherit_from_parent(style, *parent_style, own.overrides);
        // Custom properties inherit; the element's own values take precedence.
        for (const auto& [name, value] : parent_style->custom_properties) {
            style.custom_properties.emplace(name, value);
        }
    }

    // font-family is final here (cascade + inheritance done); resolve any
    // matching @font-face to its loadable key so paint uses the web font.
    if (fonts && !fonts->empty()) {
        style.font_src = fonts->resolve(style.font_face);
    }

    node->set_computed_style(std::make_shared<ComputedStyle>(std::move(style)));

    for (const auto& child : node->get_children()) {
        compute_node(index, child.get(), node->get_computed_style().get(), fonts);
    }
}

}  // namespace

void StyleEngine::apply(const Stylesheet& sheet, DOM::Node* root, const MediaContext& media,
                        const FontFaceRegistry* fonts) {
    if (!root) return;
    // Index the sheet once per apply (bucketed by key selector), then walk the
    // tree testing only candidate rules per element instead of the whole sheet.
    const RuleIndex index = build_rule_index(sheet, media);
    compute_node(index, root, nullptr, fonts);
}

}  // namespace Hummingbird::Css
