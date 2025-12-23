#include "layout/TreeBuilder.h"

#include "core/dom/Element.h"
#include "core/dom/Text.h"
#include "layout/BlockBox.h"
#include "layout/InlineBox.h"
#include "layout/RenderBreak.h"
#include "layout/RenderRule.h"
#include "layout/TextBox.h"
#include "style/ComputedStyle.h"

namespace Hummingbird::Layout {

std::unique_ptr<RenderObject> create_render_object(const DOM::Node* node) {
    if (auto element_node = dynamic_cast<const DOM::Element*>(node)) {
        // Skip non-visual elements for now.
        const auto& tag = element_node->get_tag_name();
        if (tag == "head" || tag == "style" || tag == "title" || tag == "script") {
            return nullptr;
        }
        if (tag == "a" || tag == "span" || tag == "strong" || tag == "em" || tag == "b" || tag == "i" ||
            tag == "code") {
            return std::make_unique<InlineBox>(element_node);
        }
        if (tag == "br") {
            return std::make_unique<RenderBreak>(element_node);
        }
        if (tag == "hr") {
            return std::make_unique<RenderRule>(element_node);
        }
        // For now, all other elements are treated as block boxes.
        return std::make_unique<BlockBox>(element_node);
    } else if (auto text_node = dynamic_cast<const DOM::Text*>(node)) {
        // Preserve whitespace-only text nodes; TextBox will collapse appropriately.
        if (!text_node->get_text().empty()) {
            return std::make_unique<TextBox>(text_node);
        }
    }
    return nullptr;
}

bool is_display_none(const DOM::Node* node) {
    if (!node) return false;
    auto style = node->get_computed_style();
    return style && style->display == Css::ComputedStyle::Display::None;
}

std::unique_ptr<RenderObject> build_recursive(const DOM::Node* node) {
    if (!node) {
        return nullptr;
    }

    if (is_display_none(node)) {
        return nullptr;
    }

    auto render_object = create_render_object(node);

    // If the current DOM node doesn't produce a render object (e.g., whitespace text node),
    // we skip this node entirely.
    if (!render_object) {
        return nullptr;
    }

    for (const auto& child_dom : node->get_children()) {
        auto child_render_object = build_recursive(child_dom.get());
        if (child_render_object) {
            render_object->append_child(std::move(child_render_object));
        }
    }

    return render_object;
}

std::unique_ptr<RenderObject> TreeBuilder::build_impl(const DOM::Node* dom_root) {
    if (!dom_root) return nullptr;

    // Always return a render root to host visible children, even if the root DOM node itself is non-visual.
    auto render_root = create_render_object(dom_root);
    if (!render_root) {
        render_root = std::make_unique<BlockBox>(dom_root);
    }

    for (const auto& child_dom : dom_root->get_children()) {
        if (is_display_none(child_dom.get())) {
            continue;
        }
        auto child_render_object = build_recursive(child_dom.get());
        if (child_render_object) {
            render_root->append_child(std::move(child_render_object));
        }
    }

    return render_root;
}

}  // namespace Hummingbird::Layout
