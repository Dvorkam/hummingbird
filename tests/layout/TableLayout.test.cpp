#include <gtest/gtest.h>

#include "TestGraphicsContext.h"
#include "core/ArenaAllocator.h"
#include "core/dom/DomFactory.h"
#include "core/dom/Element.h"
#include "core/dom/Text.h"
#include "html/HtmlTagNames.h"
#include "layout/RenderTable.h"
#include "layout/TreeBuilder.h"
#include "style/StyleEngine.h"

using namespace Hummingbird::Layout;
using namespace Hummingbird::DOM;
using namespace Hummingbird::Css;
namespace TagNames = Hummingbird::Html::TagNames;

TEST(TableLayoutTest, AlignsCellsIntoColumns) {
    ArenaAllocator arena(4096);
    auto body = DomFactory::create_element(arena, TagNames::Body);
    auto table = DomFactory::create_element(arena, TagNames::Table);
    auto row1 = DomFactory::create_element(arena, TagNames::Tr);
    auto cell11 = DomFactory::create_element(arena, TagNames::Td);
    cell11->append_child(DomFactory::create_text(arena, "AAA"));
    auto cell12 = DomFactory::create_element(arena, TagNames::Td);
    cell12->append_child(DomFactory::create_text(arena, "B"));
    row1->append_child(std::move(cell11));
    row1->append_child(std::move(cell12));
    table->append_child(std::move(row1));
    auto row2 = DomFactory::create_element(arena, TagNames::Tr);
    auto cell21 = DomFactory::create_element(arena, TagNames::Td);
    cell21->append_child(DomFactory::create_text(arena, "C"));
    auto cell22 = DomFactory::create_element(arena, TagNames::Td);
    cell22->append_child(DomFactory::create_text(arena, "DDDD"));
    row2->append_child(std::move(cell21));
    row2->append_child(std::move(cell22));
    table->append_child(std::move(row2));
    body->append_child(std::move(table));

    Stylesheet sheet;
    StyleEngine engine;
    engine.apply(sheet, body.get());

    TreeBuilder builder;
    auto render_root = builder.build(body.get());
    ASSERT_NE(render_root, nullptr);
    ASSERT_EQ(render_root->get_children().size(), 1u);

    TestGraphicsContext context;
    Rect viewport{0, 0, 400, 200};
    render_root->layout(context, viewport);

    auto* table_render = dynamic_cast<RenderTable*>(render_root->get_children()[0].get());
    ASSERT_NE(table_render, nullptr);
    ASSERT_EQ(table_render->get_children().size(), 2u);

    auto* first_row = dynamic_cast<RenderTableRow*>(table_render->get_children()[0].get());
    auto* second_row = dynamic_cast<RenderTableRow*>(table_render->get_children()[1].get());
    ASSERT_NE(first_row, nullptr);
    ASSERT_NE(second_row, nullptr);
    ASSERT_EQ(first_row->get_children().size(), 2u);
    ASSERT_EQ(second_row->get_children().size(), 2u);

    auto* cell11_render = dynamic_cast<RenderTableCell*>(first_row->get_children()[0].get());
    auto* cell12_render = dynamic_cast<RenderTableCell*>(first_row->get_children()[1].get());
    auto* cell21_render = dynamic_cast<RenderTableCell*>(second_row->get_children()[0].get());
    auto* cell22_render = dynamic_cast<RenderTableCell*>(second_row->get_children()[1].get());
    ASSERT_NE(cell11_render, nullptr);
    ASSERT_NE(cell12_render, nullptr);
    ASSERT_NE(cell21_render, nullptr);
    ASSERT_NE(cell22_render, nullptr);

    EXPECT_FLOAT_EQ(cell11_render->get_rect().width, 24.0f);
    EXPECT_FLOAT_EQ(cell12_render->get_rect().width, 32.0f);
    EXPECT_FLOAT_EQ(cell21_render->get_rect().width, 24.0f);
    EXPECT_FLOAT_EQ(cell22_render->get_rect().width, 32.0f);

    EXPECT_FLOAT_EQ(cell12_render->get_rect().x, cell11_render->get_rect().width);
    EXPECT_FLOAT_EQ(cell22_render->get_rect().x, cell21_render->get_rect().width);
    EXPECT_FLOAT_EQ(second_row->get_rect().y, first_row->get_rect().height);
}
