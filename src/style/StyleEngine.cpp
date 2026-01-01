#include "style/StyleEngine.h"

#include <algorithm>
#include <unordered_map>

#include "core/dom/Element.h"
#include "core/dom/Node.h"
#include "html/HtmlTagNames.h"
#include "style/CssValueNames.h"
#include "style/SelectorMatcher.h"

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

struct StyleOverrides {
    bool color = false;
    bool underline = false;
    bool whitespace = false;
    bool font_monospace = false;
    bool weight = false;
    bool style = false;
    bool font_size = false;
    bool background = false;
};

struct StyleResult {
    ComputedStyle style;
    StyleOverrides overrides;
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
    if (it != properties.end()) {
        target = value_to_length(it->second.value, target.value_or(0.0f));
    }
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

void apply_color_property(const PropertyMap& properties, ComputedStyle& style, StyleOverrides& overrides) {
    auto color_it = properties.find(Property::Color);
    if (color_it != properties.end() && color_it->second.value.type == Value::Type::Color) {
        style.color = color_it->second.value.color;
        overrides.color = true;
    }
}

void apply_properties_to_style(const PropertyMap& properties, ComputedStyle& style, StyleOverrides& overrides,
                               bool& display_set) {
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
    apply_length_if_present(properties, Property::MarginRight, style.margin.right);
    apply_length_if_present(properties, Property::MarginBottom, style.margin.bottom);
    apply_length_if_present(properties, Property::MarginLeft, style.margin.left);

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

    apply_color_property(properties, style, overrides);
}

void apply_ua_defaults(const DOM::Element& element, ComputedStyle& style, StyleOverrides& overrides, bool display_set) {
    const auto& tag = element.get_tag_name();
    const auto set_heading = [&](float scale, float margin_em) {
        style.font_size = 16.0f * scale;
        style.weight = ComputedStyle::FontWeight::Bold;
        float m = style.font_size * margin_em;
        style.margin.top = style.margin.bottom = m;
    };

    if (!display_set) {
        if (tag == Hummingbird::Html::TagNames::A || tag == Hummingbird::Html::TagNames::Span ||
            tag == Hummingbird::Html::TagNames::Strong || tag == Hummingbird::Html::TagNames::Em ||
            tag == Hummingbird::Html::TagNames::B || tag == Hummingbird::Html::TagNames::I ||
            tag == Hummingbird::Html::TagNames::Code) {
            style.display = ComputedStyle::Display::Inline;
        } else if (tag == Hummingbird::Html::TagNames::Li) {
            style.display = ComputedStyle::Display::ListItem;
        }
    }

    if (tag == Hummingbird::Html::TagNames::Ul || tag == Hummingbird::Html::TagNames::Ol) {
        style.padding.left = 20.0f;
    } else if (tag == Hummingbird::Html::TagNames::Pre) {
        if (style.whitespace == ComputedStyle::WhiteSpace::Normal) {
            style.whitespace = ComputedStyle::WhiteSpace::Preserve;
        }
        style.font_monospace = true;
        overrides.whitespace = true;
        overrides.font_monospace = true;
    } else if (tag == Hummingbird::Html::TagNames::A) {
        style.color = {0, 0, 255, 255};
        style.underline = true;
        overrides.color = true;
        overrides.underline = true;
    } else if (tag == Hummingbird::Html::TagNames::Code) {
        style.font_monospace = true;
        style.background = Color{230, 230, 230, 255};
        style.padding.left = style.padding.right = 2.0f;
        style.padding.top = style.padding.bottom = 1.0f;
        overrides.font_monospace = true;
        overrides.background = true;
    } else if (tag == Hummingbird::Html::TagNames::Blockquote) {
        style.margin.left = 40.0f;
        style.margin.right = 40.0f;
        style.margin.top = 8.0f;
        style.margin.bottom = 8.0f;
    } else if (tag == Hummingbird::Html::TagNames::Hr) {
        style.height = 2.0f;
        style.margin.top = style.margin.bottom = 8.0f;
        style.background = Color{50, 50, 50, 255};
    } else if (tag == Hummingbird::Html::TagNames::Strong) {
        style.weight = ComputedStyle::FontWeight::Bold;
        overrides.weight = true;
    } else if (tag == Hummingbird::Html::TagNames::Em) {
        style.style = ComputedStyle::FontStyle::Italic;
        overrides.style = true;
    } else if (tag == Hummingbird::Html::TagNames::H1) {
        set_heading(2.0f, 0.67f);
        overrides.font_size = true;
        overrides.weight = true;
    } else if (tag == Hummingbird::Html::TagNames::H2) {
        set_heading(1.5f, 0.83f);
        overrides.font_size = true;
        overrides.weight = true;
    } else if (tag == Hummingbird::Html::TagNames::H3) {
        set_heading(1.17f, 1.0f);
        overrides.font_size = true;
        overrides.weight = true;
    } else if (tag == Hummingbird::Html::TagNames::H4) {
        set_heading(1.0f, 1.33f);
        overrides.font_size = true;
        overrides.weight = true;
    } else if (tag == Hummingbird::Html::TagNames::H5) {
        set_heading(0.83f, 1.67f);
        overrides.font_size = true;
        overrides.weight = true;
    } else if (tag == Hummingbird::Html::TagNames::H6) {
        set_heading(0.67f, 2.33f);
        overrides.font_size = true;
        overrides.weight = true;
    }
}

void apply_non_inheritable(ComputedStyle& target, const ComputedStyle& source) {
    target.margin = source.margin;
    target.padding = source.padding;
    target.width = source.width;
    target.height = source.height;
    target.display = source.display;
    target.border_width = source.border_width;
    target.border_color = source.border_color;
    target.border_style = source.border_style;
}

void apply_inheritable_overrides(ComputedStyle& target, const ComputedStyle& source, const StyleOverrides& overrides) {
    if (overrides.color) target.color = source.color;
    if (overrides.underline) target.underline = source.underline;
    if (overrides.whitespace) target.whitespace = source.whitespace;
    if (overrides.font_monospace) target.font_monospace = source.font_monospace;
    if (overrides.weight) target.weight = source.weight;
    if (overrides.style) target.style = source.style;
    if (overrides.font_size) target.font_size = source.font_size;
    if (overrides.background) target.background = source.background;
}

// Returns a computed style based on matching rules and parent style (for inheritance in the future).
StyleResult build_style_for(const Stylesheet& sheet, const DOM::Node* node) {
    StyleResult result{default_computed_style(), {}};
    ComputedStyle& style = result.style;
    PropertyMap properties = collect_matched_properties(sheet, node);
    bool display_set = properties.find(Property::Display) != properties.end();

    // Minimal UA defaults for basic HTML readability.
    if (const auto* element = dynamic_cast<const DOM::Element*>(node)) {
        apply_ua_defaults(*element, style, result.overrides, display_set);
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
