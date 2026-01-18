#include "style/StyleEngine.h"

#include <stddef.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/ArenaAllocator.h"
#include "core/dom/Element.h"
#include "core/dom/Node.h"
#include "core/platform_api/IGraphicsContext.h"
#include "style/ComputedStyle.h"
#include "style/CssValueNames.h"
#include "style/SelectorMatcher.h"
#include "style/StyleDefaults.h"
#include "style/Stylesheet.h"

namespace Hummingbird::Css {

namespace {

float value_to_length(const Value& value, float fallback = 0.0f) {
    if (value.type != Value::Type::Length) return fallback;
    if (value.length.unit != Unit::Px) return fallback;
    return value.length.value;
}

void apply_edge(EdgeSizes& edges, float value) {
    edges.top = edges.right = edges.bottom = edges.left = value;
}

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

PropertyMap collect_matched_properties(const Stylesheet& sheet, const DOM::Node* node) {
    PropertyMap properties;
    size_t order = 0;

    const auto* element = dynamic_cast<const DOM::Element*>(node);
    if (!element) return properties;

    for (const auto& rule : sheet.rules) {
        for (const auto& selector : rule.selectors) {
            if (!matches_selector(node, selector)) continue;
            int spec = selector.specificity();
            for (const auto& decl : rule.declarations) {
                auto it = properties.find(decl.property);
                if (it == properties.end() || spec > it->second.specificity ||
                    (spec == it->second.specificity && order > it->second.order)) {
                    properties[decl.property] = {spec, order, decl.value};
                }
                ++order;
            }
        }
    }

    return properties;
}

void apply_length_if_present(const PropertyMap& properties, Property property, float& target) {
    auto it = properties.find(property);
    if (it != properties.end()) {
        target = value_to_length(it->second.value, target);
    }
}

void apply_optional_length_if_present(const PropertyMap& properties, Property property, std::optional<float>& target) {
    auto it = properties.find(property);
    if (it == properties.end()) {
        return;
    }
    const auto& value = it->second.value;
    if (value.type != Value::Type::Length || value.length.unit != Unit::Px) {
        return;
    }
    target = value.length.value;
}

void apply_margin_if_present(const PropertyMap& properties, Property property, float& target, bool& auto_flag) {
    auto it = properties.find(property);
    if (it == properties.end()) {
        return;
    }
    const auto& value = it->second.value;
    if (value.type == Value::Type::Identifier && value.ident == ValueNames::Auto) {
        auto_flag = true;
        target = 0.0f;
        return;
    }
    auto_flag = false;
    target = value_to_length(value, target);
}

void apply_border_style(ComputedStyle& style, const Value& value) {
    if (value.type != Value::Type::Identifier) return;
    if (value.ident == ValueNames::Solid) {
        style.border_style = ComputedStyle::BorderStyle::Solid;
    }
}

bool apply_display_property(const PropertyMap& properties, ComputedStyle& style) {
    auto display_it = properties.find(Property::Display);
    if (display_it == properties.end() || display_it->second.value.type != Value::Type::Identifier) {
        return false;
    }

    const auto& ident = display_it->second.value.ident;
    if (ident == ValueNames::None) {
        style.display = ComputedStyle::Display::None;
    } else if (ident == ValueNames::Inline) {
        style.display = ComputedStyle::Display::Inline;
    } else if (ident == ValueNames::InlineBlock) {
        style.display = ComputedStyle::Display::InlineBlock;
    } else if (ident == ValueNames::ListItem) {
        style.display = ComputedStyle::Display::ListItem;
    } else if (ident == ValueNames::Block) {
        style.display = ComputedStyle::Display::Block;
    }
    return true;
}

void apply_color_property(const PropertyMap& properties, ComputedStyle& style,
                          StyleDefaults::StyleOverrides& overrides) {
    auto color_it = properties.find(Property::Color);
    if (color_it != properties.end() && color_it->second.value.type == Value::Type::Color) {
        style.color = color_it->second.value.color;
        overrides.color = true;
    }
}

void apply_background_property(const PropertyMap& properties, ComputedStyle& style,
                               StyleDefaults::StyleOverrides& overrides) {
    auto bg_it = properties.find(Property::BackgroundColor);
    if (bg_it != properties.end() && bg_it->second.value.type == Value::Type::Color) {
        style.background = bg_it->second.value.color;
        overrides.background = true;
    }
}

void apply_properties_to_style(const PropertyMap& properties, ComputedStyle& style,
                               StyleDefaults::StyleOverrides& overrides, bool& display_set) {
    display_set = apply_display_property(properties, style);

    // margin / padding shorthand and individual edges
    auto margin_it = properties.find(Property::Margin);
    if (margin_it != properties.end()) {
        apply_edge(style.margin, value_to_length(margin_it->second.value, 0.0f));
    }
    auto padding_it = properties.find(Property::Padding);
    if (padding_it != properties.end()) {
        apply_edge(style.padding, value_to_length(padding_it->second.value, 0.0f));
    }

    apply_length_if_present(properties, Property::MarginTop, style.margin.top);
    apply_margin_if_present(properties, Property::MarginRight, style.margin.right, style.margin_right_auto);
    apply_length_if_present(properties, Property::MarginBottom, style.margin.bottom);
    apply_margin_if_present(properties, Property::MarginLeft, style.margin.left, style.margin_left_auto);

    apply_length_if_present(properties, Property::PaddingTop, style.padding.top);
    apply_length_if_present(properties, Property::PaddingRight, style.padding.right);
    apply_length_if_present(properties, Property::PaddingBottom, style.padding.bottom);
    apply_length_if_present(properties, Property::PaddingLeft, style.padding.left);

    auto border_width_it = properties.find(Property::BorderWidth);
    if (border_width_it != properties.end()) {
        apply_edge(style.border_width, value_to_length(border_width_it->second.value, 0.0f));
    }
    auto border_color_it = properties.find(Property::BorderColor);
    if (border_color_it != properties.end() && border_color_it->second.value.type == Value::Type::Color) {
        style.border_color = border_color_it->second.value.color;
    }
    auto border_style_it = properties.find(Property::BorderStyle);
    if (border_style_it != properties.end()) {
        apply_border_style(style, border_style_it->second.value);
    }

    apply_optional_length_if_present(properties, Property::Width, style.width);
    apply_optional_length_if_present(properties, Property::Height, style.height);
    apply_optional_length_if_present(properties, Property::MaxWidth, style.max_width);

    auto font_size_it = properties.find(Property::FontSize);
    if (font_size_it != properties.end()) {
        if (font_size_it->second.value.type == Value::Type::Length &&
            font_size_it->second.value.length.unit == Unit::Px) {
            style.font_size = font_size_it->second.value.length.value;
            overrides.font_size = true;
        }
    }

    auto line_height_it = properties.find(Property::LineHeight);
    if (line_height_it != properties.end()) {
        const auto& value = line_height_it->second.value;
        if (value.type == Value::Type::Length && value.length.unit == Unit::Px) {
            style.line_height = value.length.value;
            overrides.line_height = true;
        } else if (value.type == Value::Type::Number) {
            style.line_height = value.number * style.font_size;
            overrides.line_height = true;
        }
    }

    apply_color_property(properties, style, overrides);
    apply_background_property(properties, style, overrides);
}

void apply_non_inheritable(ComputedStyle& target, const ComputedStyle& source) {
    target.margin = source.margin;
    target.margin_left_auto = source.margin_left_auto;
    target.margin_right_auto = source.margin_right_auto;
    target.padding = source.padding;
    target.width = source.width;
    target.height = source.height;
    target.max_width = source.max_width;
    target.display = source.display;
    target.border_width = source.border_width;
    target.border_color = source.border_color;
    target.border_style = source.border_style;
    target.background = source.background;
}

void apply_inheritable_overrides(ComputedStyle& target, const ComputedStyle& source,
                                 const StyleDefaults::StyleOverrides& overrides) {
    if (overrides.color) target.color = source.color;
    if (overrides.underline) target.underline = source.underline;
    if (overrides.whitespace) target.whitespace = source.whitespace;
    if (overrides.font_monospace) target.font_monospace = source.font_monospace;
    if (overrides.weight) target.weight = source.weight;
    if (overrides.style) target.style = source.style;
    if (overrides.font_size) target.font_size = source.font_size;
    if (overrides.font_face) target.font_face = source.font_face;
    if (overrides.text_align) target.text_align = source.text_align;
    if (overrides.background) target.background = source.background;
    if (overrides.line_height) target.line_height = source.line_height;
}

// Returns a computed style based on matching rules and parent style (for inheritance in the future).
StyleResult build_style_for(const Stylesheet& sheet, const DOM::Node* node) {
    StyleResult result{default_computed_style(), {}};
    ComputedStyle& style = result.style;
    PropertyMap properties = collect_matched_properties(sheet, node);
    bool display_set = properties.find(Property::Display) != properties.end();

    // Minimal UA defaults for basic HTML readability.
    if (const auto* element = dynamic_cast<const DOM::Element*>(node)) {
        StyleDefaults::apply_user_agent_defaults(*element, style, result.overrides, display_set);
        StyleDefaults::apply_legacy_attributes(*element, style, result.overrides);
    }

    apply_properties_to_style(properties, style, result.overrides, display_set);

    return result;
}

}  // namespace

void StyleEngine::compute_node(const Stylesheet& sheet, DOM::Node* node, const ComputedStyle* parent_style) {
    ComputedStyle base = parent_style ? *parent_style : default_computed_style();
    StyleResult own = build_style_for(sheet, node);

    // Non-inheritable box properties come from the computed (own) style.
    apply_non_inheritable(base, own.style);

    // Inheritable text properties: only elements introduce overrides; text nodes inherit.
    if (dynamic_cast<DOM::Element*>(node)) {
        apply_inheritable_overrides(base, own.style, own.overrides);
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
