#include "style/StyleEngine.h"
#include "style/SelectorMatcher.h"
#include "core/dom/Node.h"
#include "core/dom/Element.h"
#include <unordered_map>
#include <algorithm>

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
                if (it == properties.end() || spec > it->second.specificity || (spec == it->second.specificity && order > it->second.order)) {
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
        if (tag == "ul") {
            style.padding.left = 20.0f;
        } else if (tag == "pre") {
            if (style.whitespace == ComputedStyle::WhiteSpace::Normal) {
                style.whitespace = ComputedStyle::WhiteSpace::Preserve;
            }
            style.font_monospace = true; // For future use by renderers that pick fonts.
        } else if (tag == "a") {
            style.color = {0, 0, 255, 255};
            style.underline = true;
        }
    }

    return style;
}

}

void StyleEngine::compute_node(const Stylesheet& sheet, DOM::Node* node, const ComputedStyle* parent_style) {
    ComputedStyle style = build_style_for(sheet, node);
    // Inherit nothing for now; hook for future inherited properties via parent_style.
    node->set_computed_style(std::make_shared<ComputedStyle>(style));

    for (const auto& child : node->get_children()) {
        compute_node(sheet, child.get(), node->get_computed_style().get());
    }
}

void StyleEngine::apply(const Stylesheet& sheet, DOM::Node* root) {
    if (!root) return;
    compute_node(sheet, root, nullptr);
}

} // namespace Hummingbird::Css
