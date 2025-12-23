#include "style/StyleEngine.h"

#include <algorithm>
#include <unordered_map>

#include "core/dom/Element.h"
#include "core/dom/Node.h"
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

struct PropertyHash {
    size_t operator()(Property property) const { return static_cast<size_t>(property); }
};

// Returns a computed style based on matching rules and parent style (for inheritance in the future).
ComputedStyle build_style_for(const Stylesheet& sheet, const DOM::Node* node) {
    ComputedStyle style = default_computed_style();
    std::unordered_map<Property, MatchedProperty, PropertyHash> properties;
    size_t order = 0;

    // Only elements participate in selector matching.
    const auto* element = dynamic_cast<const DOM::Element*>(node);
    if (element) {
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
    }

    auto apply_length_if_present = [&](Property property, float& target) {
        auto it = properties.find(property);
        if (it != properties.end()) {
            target = value_to_length(it->second.value, target);
        }
    };

    auto apply_optional_length_if_present = [&](Property property, std::optional<float>& target) {
        auto it = properties.find(property);
        if (it != properties.end()) {
            target = value_to_length(it->second.value, target.value_or(0.0f));
        }
    };

    auto apply_border_style = [&](const Value& value) {
        if (value.type != Value::Type::Identifier) return;
        if (value.ident == "solid") {
            style.border_style = ComputedStyle::BorderStyle::Solid;
        }
    };

    bool display_set = properties.find(Property::Display) != properties.end();

    // margin / padding shorthand and individual edges
    auto margin_it = properties.find(Property::Margin);
    if (margin_it != properties.end()) {
        apply_edge(style.margin, value_to_length(margin_it->second.value, 0.0f));
    }
    auto padding_it = properties.find(Property::Padding);
    if (padding_it != properties.end()) {
        apply_edge(style.padding, value_to_length(padding_it->second.value, 0.0f));
    }

    apply_length_if_present(Property::MarginTop, style.margin.top);
    apply_length_if_present(Property::MarginRight, style.margin.right);
    apply_length_if_present(Property::MarginBottom, style.margin.bottom);
    apply_length_if_present(Property::MarginLeft, style.margin.left);

    apply_length_if_present(Property::PaddingTop, style.padding.top);
    apply_length_if_present(Property::PaddingRight, style.padding.right);
    apply_length_if_present(Property::PaddingBottom, style.padding.bottom);
    apply_length_if_present(Property::PaddingLeft, style.padding.left);

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
        apply_border_style(border_style_it->second.value);
    }

    apply_optional_length_if_present(Property::Width, style.width);
    apply_optional_length_if_present(Property::Height, style.height);

    auto display_it = properties.find(Property::Display);
    if (display_it != properties.end() && display_it->second.value.type == Value::Type::Identifier) {
        if (display_it->second.value.ident == "none") {
            style.display = ComputedStyle::Display::None;
        } else if (display_it->second.value.ident == "inline") {
            style.display = ComputedStyle::Display::Inline;
        } else if (display_it->second.value.ident == "inline-block") {
            style.display = ComputedStyle::Display::InlineBlock;
        } else if (display_it->second.value.ident == "block") {
            style.display = ComputedStyle::Display::Block;
        }
    }

    auto color_it = properties.find(Property::Color);
    if (color_it != properties.end() && color_it->second.value.type == Value::Type::Color) {
        style.color = color_it->second.value.color;
    }

    // Minimal UA defaults for basic HTML readability.
    if (element) {
        const auto& tag = element->get_tag_name();
        const auto set_heading = [&](float scale, float margin_em) {
            style.font_size = 16.0f * scale;
            style.weight = ComputedStyle::FontWeight::Bold;
            float m = style.font_size * margin_em;
            style.margin.top = style.margin.bottom = m;
        };

        if (!display_set) {
            if (tag == "a" || tag == "span" || tag == "strong" || tag == "em" || tag == "b" || tag == "i" ||
                tag == "code") {
                style.display = ComputedStyle::Display::Inline;
            }
        }

        if (tag == "ul" || tag == "ol") {
            style.padding.left = 20.0f;
        } else if (tag == "pre") {
            if (style.whitespace == ComputedStyle::WhiteSpace::Normal) {
                style.whitespace = ComputedStyle::WhiteSpace::Preserve;
            }
            style.font_monospace = true;
        } else if (tag == "a") {
            style.color = {0, 0, 255, 255};
            style.underline = true;
        } else if (tag == "code") {
            style.font_monospace = true;
            style.background = Color{230, 230, 230, 255};
            style.padding.left = style.padding.right = 2.0f;
            style.padding.top = style.padding.bottom = 1.0f;
        } else if (tag == "blockquote") {
            style.margin.left = 40.0f;
            style.margin.right = 40.0f;
            style.margin.top = 8.0f;
            style.margin.bottom = 8.0f;
        } else if (tag == "hr") {
            style.height = 2.0f;
            style.margin.top = style.margin.bottom = 8.0f;
            style.background = Color{50, 50, 50, 255};
        } else if (tag == "strong") {
            style.weight = ComputedStyle::FontWeight::Bold;
        } else if (tag == "em") {
            style.style = ComputedStyle::FontStyle::Italic;
        } else if (tag == "h1") {
            set_heading(2.0f, 0.67f);
        } else if (tag == "h2") {
            set_heading(1.5f, 0.83f);
        } else if (tag == "h3") {
            set_heading(1.17f, 1.0f);
        } else if (tag == "h4") {
            set_heading(1.0f, 1.33f);
        } else if (tag == "h5") {
            set_heading(0.83f, 1.67f);
        } else if (tag == "h6") {
            set_heading(0.67f, 2.33f);
        }
    }

    return style;
}

}  // namespace

void StyleEngine::compute_node(const Stylesheet& sheet, DOM::Node* node, const ComputedStyle* parent_style) {
    ComputedStyle base = parent_style ? *parent_style : default_computed_style();
    ComputedStyle own = build_style_for(sheet, node);

    // Non-inheritable box properties come from the computed (own) style.
    base.margin = own.margin;
    base.padding = own.padding;
    base.width = own.width;
    base.height = own.height;
    base.display = own.display;
    base.border_width = own.border_width;
    base.border_color = own.border_color;
    base.border_style = own.border_style;

    // Inheritable text properties: only elements introduce overrides; text nodes inherit.
    if (dynamic_cast<DOM::Element*>(node)) {
        base.color = own.color;
        base.underline = own.underline;
        base.whitespace = own.whitespace;
        base.font_monospace = own.font_monospace;
        base.weight = own.weight;
        base.style = own.style;
        base.font_size = own.font_size;
        base.background = own.background;
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
