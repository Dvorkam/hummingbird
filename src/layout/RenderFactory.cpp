#include "layout/RenderFactory.h"

#include "layout/BlockBox.h"
#include "layout/InlineBox.h"
#include "layout/RenderBreak.h"
#include "layout/RenderImage.h"
#include "layout/RenderListItem.h"
#include "layout/RenderRule.h"
#include "layout/RenderTable.h"
#include "layout/TextBox.h"

namespace Hummingbird::Layout {

std::unique_ptr<RenderObject> RenderFactory::create_block_box(const DOM::Node* dom_node) {
    return BlockBox::create(dom_node);
}

std::unique_ptr<RenderObject> RenderFactory::create_inline_box(const DOM::Node* dom_node) {
    return InlineBox::create(dom_node);
}

std::unique_ptr<RenderObject> RenderFactory::create_inline_block_box(const DOM::Node* dom_node) {
    return InlineBlockBox::create(dom_node);
}

std::unique_ptr<RenderObject> RenderFactory::create_list_item(const DOM::Node* dom_node) {
    return RenderListItem::create(dom_node);
}

std::unique_ptr<RenderObject> RenderFactory::create_break(const DOM::Node* dom_node) {
    return RenderBreak::create(dom_node);
}

std::unique_ptr<RenderObject> RenderFactory::create_rule(const DOM::Node* dom_node) {
    return RenderRule::create(dom_node);
}

std::unique_ptr<RenderObject> RenderFactory::create_text_box(const DOM::Text* dom_node) {
    return TextBox::create(dom_node);
}

std::unique_ptr<RenderObject> RenderFactory::create_image(const DOM::Element* dom_node) {
    return RenderImage::create(dom_node);
}

std::unique_ptr<RenderObject> RenderFactory::create_table(const DOM::Node* dom_node) {
    return RenderTable::create(dom_node);
}

std::unique_ptr<RenderObject> RenderFactory::create_table_section(const DOM::Node* dom_node) {
    return RenderTableSection::create(dom_node);
}

std::unique_ptr<RenderObject> RenderFactory::create_table_row(const DOM::Node* dom_node) {
    return RenderTableRow::create(dom_node);
}

std::unique_ptr<RenderObject> RenderFactory::create_table_cell(const DOM::Node* dom_node) {
    return RenderTableCell::create(dom_node);
}

}  // namespace Hummingbird::Layout
