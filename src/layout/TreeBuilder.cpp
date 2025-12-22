#include "layout/TreeBuilder.h"

#include "core/dom/Element.h"
#include "core/dom/Text.h"
#include "layout/BlockBox.h"
#include "layout/InlineBox.h"
#include "layout/RenderBreak.h"
#include "layout/RenderRule.h"
#include "layout/TextBox.h"

namespace Hummingbird::Layout {

// Forward declaration for the recursive build helper
std::unique_ptr<RenderObject> build_recursive(const DOM::Node* node);

std::unique_ptr<RenderObject> TreeBuilder::build_impl(const DOM::Node* dom_root) {
    return build_recursive(dom_root);
}

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
        // Don't create render objects for whitespace-only text nodes
        if (text_node->get_text().find_first_not_of(" \t\n\r") != std::string::npos) {
            return std::make_unique<TextBox>(text_node);
        }
    }
    return nullptr;
}

std::unique_ptr<RenderObject> build_recursive(const DOM::Node* node) {
    if (!node) {
        return nullptr;
    }

    // 1. Create a render object for the current node.
    auto render_object = create_render_object(node);

    // If the current DOM node doesn't produce a render object (e.g., whitespace text node),
    // we don't want it in the render tree.
    if (!render_object) {
        return nullptr;
    }

    // 2. Recursively build for all children and append them.
    for (const auto& child_dom : node->get_children()) {
        auto child_render_object = build_recursive(child_dom.get());
        if (child_render_object) {
            render_object->append_child(std::move(child_render_object));
        }
    }

    return render_object;
}

}  // namespace Hummingbird::Layout
