#include "style/compute/StyleEngine.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <unordered_map>

#include "core/ArenaAllocator.h"
#include "core/dom/Element.h"
#include "core/dom/Node.h"
#include "core/platform_api/IGraphicsContext.h"
#include "style/compute/ComputedStyle.h"
#include "style/compute/StyleDefaults.h"
#include "style/compute/Stylesheet.h"
#include "style/compute/apply/PropertyApplier.h"
#include "style/registry/CssPropertyRegistry.h"
#include "style/selector/SelectorMatcher.h"

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

MatchedDeclarations collect_matched_properties(const Stylesheet& sheet, const DOM::Node* node) {
    MatchedDeclarations matched;
    size_t order = 0;

    const auto* element = dynamic_cast<const DOM::Element*>(node);
    if (!element) return matched;

    for (const auto& rule : sheet.rules) {
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

void apply_non_inheritable(ComputedStyle& target, const ComputedStyle& source) {
    target.margin = source.margin;
    target.margin_left_auto = source.margin_left_auto;
    target.margin_right_auto = source.margin_right_auto;
    target.padding = source.padding;
    target.box_sizing = source.box_sizing;
    target.transform_has_translate = source.transform_has_translate;
    target.transform_translate_x = source.transform_translate_x;
    target.transform_translate_y = source.transform_translate_y;
    target.width = source.width;
    target.height = source.height;
    target.min_width = source.min_width;
    target.min_height = source.min_height;
    target.max_width = source.max_width;
    target.max_height = source.max_height;
    target.display = source.display;
    target.border_width = source.border_width;
    target.border_color = source.border_color;
    target.border_style = source.border_style;
    target.background = source.background;
    target.background_image = source.background_image;
    target.background_repeat = source.background_repeat;
    target.background_position = source.background_position;
    target.background_size = source.background_size;
    target.position = source.position;
    target.top = source.top;
    target.right = source.right;
    target.bottom = source.bottom;
    target.left = source.left;
    target.z_index = source.z_index;
    target.float_type = source.float_type;
}

void apply_inheritable_overrides(ComputedStyle& target, const ComputedStyle& source,
                                 const StyleDefaults::StyleOverrides& overrides) {
    if (overrides.color) target.color = source.color;
    if (overrides.underline) target.underline = source.underline;
    if (overrides.underline_thickness) target.underline_thickness = source.underline_thickness;
    if (overrides.underline_offset) target.underline_offset = source.underline_offset;
    if (overrides.link_color) target.link_color = source.link_color;
    if (overrides.vlink_color) target.vlink_color = source.vlink_color;
    if (overrides.whitespace) target.whitespace = source.whitespace;
    if (overrides.font_monospace) target.font_monospace = source.font_monospace;
    if (overrides.weight) target.weight = source.weight;
    if (overrides.style) target.style = source.style;
    if (overrides.font_size) target.font_size = source.font_size;
    if (overrides.font_face) target.font_face = source.font_face;
    if (overrides.text_align) target.text_align = source.text_align;
    if (overrides.background) target.background = source.background;
    if (overrides.line_height) target.line_height = source.line_height;
    if (overrides.list_style_type) target.list_style_type = source.list_style_type;
    if (overrides.list_style_position) target.list_style_position = source.list_style_position;
}

// Returns a computed style based on matching rules and parent style (for inheritance in the future).
StyleResult build_style_for(const Stylesheet& sheet, const DOM::Node* node, const ComputedStyle* parent_style) {
    StyleResult result{default_computed_style(), {}};
    ComputedStyle& style = result.style;
    MatchedDeclarations matched = collect_matched_properties(sheet, node);
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

void StyleEngine::compute_node(const Stylesheet& sheet, DOM::Node* node, const ComputedStyle* parent_style) {
    ComputedStyle base = parent_style ? *parent_style : default_computed_style();
    StyleResult own = build_style_for(sheet, node, parent_style);

    // Non-inheritable box properties come from the computed (own) style.
    apply_non_inheritable(base, own.style);

    // Inheritable text properties: only elements introduce overrides; text nodes inherit.
    if (dynamic_cast<DOM::Element*>(node)) {
        apply_inheritable_overrides(base, own.style, own.overrides);
    }

    for (const auto& [name, value] : own.style.custom_properties) {
        base.custom_properties[name] = value;
    }

    ComputedStyle style = base;

    node->set_computed_style(std::make_shared<ComputedStyle>(style));

    for (const auto& child : node->get_children()) {
        compute_node(sheet, child.get(), node->get_computed_style().get());
    }
}

void StyleEngine::apply(const Stylesheet& sheet, DOM::Node* root) {
    if (!root) return;
    compute_node(sheet, root, nullptr);
}

}  // namespace Hummingbird::Css
