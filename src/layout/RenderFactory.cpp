#include "layout/RenderFactory.h"

#include "layout/RenderObject.h"
#include "layout/block/BlockBox.h"
#include "layout/controls/RenderBreak.h"
#include "layout/controls/RenderRule.h"
#include "layout/flex/FlexBox.h"
#include "layout/flow/InlineBox.h"
#include "layout/flow/TextBox.h"
#include "layout/formatting/RenderListItem.h"
#include "layout/replaced/RenderImage.h"
#include "layout/replaced/RenderSvg.h"
#include "layout/table/RenderTable.h"

namespace Hummingbird::Layout {

std::unique_ptr<RenderObject> RenderFactory::create_block_box(const DOM::Node* dom_node) {
    return BlockBox::create(dom_node);
}

std::unique_ptr<RenderObject> RenderFactory::create_flex_box(const DOM::Node* dom_node) {
    return FlexBox::create(dom_node);
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

std::unique_ptr<RenderObject> RenderFactory::create_svg(const DOM::Element* dom_node) {
    return RenderSvg::create(dom_node);
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
