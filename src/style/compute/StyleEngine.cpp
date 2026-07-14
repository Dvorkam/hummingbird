#include "style/compute/StyleEngine.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <unordered_map>

#include "core/ArenaAllocator.h"
#include "core/dom/Element.h"
#include "core/dom/Node.h"
#include "core/platform_api/IGraphicsContext.h"
#include "style/compute/StyleDefaults.h"
#include "style/compute/Stylesheet.h"
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
};

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

MatchedDeclarations collect_matched_properties(const Stylesheet& sheet, const DOM::Node* node,
                                               const MediaContext& media) {
    MatchedDeclarations matched;
    size_t order = 0;

    const auto* element = dynamic_cast<const DOM::Element*>(node);
    if (!element) return matched;

    for (const auto& rule : sheet.rules) {
        if (rule.media && !media_condition_matches(*rule.media, media)) {
            continue;
        }
        for (const auto& selector : rule.selectors) {
            if (!matches_selector(node, selector)) continue;
            int spec = selector.specificity();
            for (const auto& decl : rule.declarations) {
                if (decl.property == Property::Custom) {
                    if (decl.custom_property.empty()) {
                        ++order;
                        continue;
                    }
                    auto it = matched.custom_properties.find(decl.custom_property);
                    if (it == matched.custom_properties.end() || spec > it->second.specificity ||
                        (spec == it->second.specificity && order > it->second.order)) {
                        matched.custom_properties[decl.custom_property] = {spec, order, decl.value};
                    }
                    ++order;
                    continue;
                }
                auto it = matched.properties.find(decl.property);
                if (it == matched.properties.end() || spec > it->second.specificity ||
                    (spec == it->second.specificity && order > it->second.order)) {
                    matched.properties[decl.property] = {spec, order, decl.value};
                }
                ++order;
            }
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
        Apply::apply_property(entry.property, it->second.value, style, overrides, context);
    }
}

void apply_custom_properties(const CustomPropertyMap& properties, ComputedStyle& style) {
    for (const auto& [name, property] : properties) {
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
}

// Returns a computed style based on matching rules and parent style (for inheritance in the future).
StyleResult build_style_for(const Stylesheet& sheet, const DOM::Node* node, const ComputedStyle* parent_style,
                            const MediaContext& media) {
    StyleResult result{default_computed_style(), {}};
    ComputedStyle& style = result.style;
    MatchedDeclarations matched = collect_matched_properties(sheet, node, media);
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

}  // namespace

void StyleEngine::compute_node(const Stylesheet& sheet, DOM::Node* node, const ComputedStyle* parent_style,
                               const MediaContext& media) {
    StyleResult own = build_style_for(sheet, node, parent_style, media);
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

    node->set_computed_style(std::make_shared<ComputedStyle>(std::move(style)));

    for (const auto& child : node->get_children()) {
        compute_node(sheet, child.get(), node->get_computed_style().get(), media);
    }
}

void StyleEngine::apply(const Stylesheet& sheet, DOM::Node* root, const MediaContext& media) {
    if (!root) return;
    compute_node(sheet, root, nullptr, media);
}

}  // namespace Hummingbird::Css
