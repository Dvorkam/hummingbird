#include "layout/TreeBuilder.h"

#include "core/dom/Element.h"
#include "core/dom/Text.h"
#include "html/HtmlTagNames.h"
#include "layout/RenderFactory.h"
#include "style/ComputedStyle.h"

namespace Hummingbird::Layout {

namespace {
bool is_whitespace_only(const std::string& text) {
    for (char c : text) {
        if (c != ' ' && c != '\n' && c != '\r' && c != '\t') {
            return false;
        }
    }
    return true;
}

bool is_non_visual_tag(std::string_view tag) {
    return tag == Hummingbird::Html::TagNames::Head || tag == Hummingbird::Html::TagNames::Style ||
           tag == Hummingbird::Html::TagNames::Title || tag == Hummingbird::Html::TagNames::Script;
}

std::unique_ptr<RenderObject> render_for_display(const DOM::Element* element, Css::ComputedStyle::Display display) {
    switch (display) {
        case Css::ComputedStyle::Display::Inline:
            return RenderFactory::create_inline_box(element);
        case Css::ComputedStyle::Display::InlineBlock:
            return RenderFactory::create_inline_block_box(element);
        case Css::ComputedStyle::Display::ListItem:
            return RenderFactory::create_list_item(element);
        case Css::ComputedStyle::Display::None:
            return nullptr;
        case Css::ComputedStyle::Display::Block:
        default:
            return RenderFactory::create_block_box(element);
    }
}

bool should_skip_text_node(const DOM::Text* text_node) {
    if (!text_node || text_node->get_text().empty()) {
        return true;
    }
    const auto& text = text_node->get_text();
    if (!is_whitespace_only(text)) {
        return false;
    }
    auto style = text_node->get_computed_style();
    return !style || style->whitespace != Css::ComputedStyle::WhiteSpace::Preserve;
}
}  // namespace

std::unique_ptr<RenderObject> create_render_object(const DOM::Node* node) {
    if (auto element_node = dynamic_cast<const DOM::Element*>(node)) {
        // Skip non-visual elements for now.
        const auto& tag = element_node->get_tag_name();
        if (is_non_visual_tag(tag)) {
            return nullptr;
        }
        if (tag == Hummingbird::Html::TagNames::Br) {
            return RenderFactory::create_break(element_node);
        }
        if (tag == Hummingbird::Html::TagNames::Hr) {
            return RenderFactory::create_rule(element_node);
        }
        if (tag == Hummingbird::Html::TagNames::Img) {
            return RenderFactory::create_image(element_node);
        }
        if (tag == Hummingbird::Html::TagNames::Table) {
            return RenderFactory::create_table(element_node);
        }
        if (tag == Hummingbird::Html::TagNames::Thead || tag == Hummingbird::Html::TagNames::Tbody ||
            tag == Hummingbird::Html::TagNames::Tfoot) {
            return RenderFactory::create_table_section(element_node);
        }
        if (tag == Hummingbird::Html::TagNames::Tr) {
            return RenderFactory::create_table_row(element_node);
        }
        if (tag == Hummingbird::Html::TagNames::Td || tag == Hummingbird::Html::TagNames::Th) {
            return RenderFactory::create_table_cell(element_node);
        }
        auto style = element_node->get_computed_style();
        if (style) {
            return render_for_display(element_node, style->display);
        }
        return RenderFactory::create_block_box(element_node);
    } else if (auto text_node = dynamic_cast<const DOM::Text*>(node)) {
        if (!should_skip_text_node(text_node)) {
            return RenderFactory::create_text_box(text_node);
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
        render_root = RenderFactory::create_block_box(dom_root);
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
