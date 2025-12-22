#include "style/StyleEngine.h"

#include <algorithm>
#include <unordered_map>

#include "core/dom/Element.h"
#include "core/dom/Node.h"
#include "style/SelectorMatcher.h"

namespace Hummingbird::Css {

namespace {

float parse_length(const std::string& value, float fallback = 0.0f) {
    if (value.empty()) return fallback;
    std::string trimmed = value;
    // Drop trailing px if present
    if (trimmed.size() > 2 && trimmed.substr(trimmed.size() - 2) == "px") {
        trimmed = trimmed.substr(0, trimmed.size() - 2);
    }
    try {
        return std::stof(trimmed);
    } catch (...) {
        return fallback;
    }
}

void apply_edge(EdgeSizes& edges, float value) {
    edges.top = edges.right = edges.bottom = edges.left = value;
}

struct MatchedProperty {
    int specificity;
    size_t order;
    std::string value;
};

// Returns a computed style based on matching rules and parent style (for inheritance in the future).
ComputedStyle build_style_for(const Stylesheet& sheet, const DOM::Node* node) {
    ComputedStyle style = default_computed_style();
    std::unordered_map<std::string, MatchedProperty> properties;
    size_t order = 0;

    // Only elements participate in selector matching.
    const auto* element = dynamic_cast<const DOM::Element*>(node);
    if (element) {
        for (const auto& rule : sheet.rules) {
            if (!matches_selector(node, rule.selector)) continue;
            int spec = rule.selector.specificity();
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

    auto apply_length_if_present = [&](const std::string& name, float& target) {
        auto it = properties.find(name);
        if (it != properties.end()) {
            target = parse_length(it->second.value, target);
        }
    };

    auto apply_optional_length_if_present = [&](const std::string& name, std::optional<float>& target) {
        auto it = properties.find(name);
        if (it != properties.end()) {
            target = parse_length(it->second.value, target.value_or(0.0f));
        }
    };

    // margin / padding shorthand and individual edges
    auto margin_it = properties.find("margin");
    if (margin_it != properties.end()) {
        apply_edge(style.margin, parse_length(margin_it->second.value, 0.0f));
    }
    auto padding_it = properties.find("padding");
    if (padding_it != properties.end()) {
        apply_edge(style.padding, parse_length(padding_it->second.value, 0.0f));
    }

    apply_length_if_present("margin-top", style.margin.top);
    apply_length_if_present("margin-right", style.margin.right);
    apply_length_if_present("margin-bottom", style.margin.bottom);
    apply_length_if_present("margin-left", style.margin.left);

    apply_length_if_present("padding-top", style.padding.top);
    apply_length_if_present("padding-right", style.padding.right);
    apply_length_if_present("padding-bottom", style.padding.bottom);
    apply_length_if_present("padding-left", style.padding.left);

    apply_optional_length_if_present("width", style.width);
    apply_optional_length_if_present("height", style.height);

    // Minimal UA defaults for basic HTML readability.
    if (element) {
        const auto& tag = element->get_tag_name();
        const auto set_heading = [&](float scale, float margin_em) {
            style.font_size = 16.0f * scale;
            style.weight = ComputedStyle::FontWeight::Bold;
            float m = style.font_size * margin_em;
            style.margin.top = style.margin.bottom = m;
        };

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
