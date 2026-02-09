#include <gtest/gtest.h>

#include "core/ArenaAllocator.h"
#include "core/dom/DomFactory.h"
#include "core/dom/Element.h"
#include "core/dom/Text.h"
#include "html/HtmlAttributeNames.h"
#include "html/HtmlTagNames.h"
#include "layout/TreeBuilder.h"
#include "layout/table/RenderTable.h"
#include "style/compute/StyleEngine.h"
#include "test_utils/TestGraphicsContext.h"

using namespace Hummingbird::Layout;
using namespace Hummingbird::DOM;
using namespace Hummingbird::Css;
namespace TagNames = Hummingbird::Html::TagNames;
namespace Attr = Hummingbird::Html::AttributeNames;

TEST(TableLayoutTest, AlignsCellsIntoColumns) {
    Hummingbird::Core::ArenaAllocator arena(4096);
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

    Hummingbird::Test::TestGraphicsContext context;
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

    EXPECT_FLOAT_EQ(cell11_render->get_rect().width, 28.0f);
    EXPECT_FLOAT_EQ(cell12_render->get_rect().width, 36.0f);
    EXPECT_FLOAT_EQ(cell21_render->get_rect().width, 28.0f);
    EXPECT_FLOAT_EQ(cell22_render->get_rect().width, 36.0f);

    EXPECT_FLOAT_EQ(cell12_render->get_rect().x, cell11_render->get_rect().width);
    EXPECT_FLOAT_EQ(cell22_render->get_rect().x, cell21_render->get_rect().width);
    EXPECT_FLOAT_EQ(second_row->get_rect().y, first_row->get_rect().height);
}

TEST(TableLayoutTest, ExpandsColumnsWhenTableIsPercentWidth) {
    Hummingbird::Core::ArenaAllocator arena(4096);
    auto body = DomFactory::create_element(arena, TagNames::Body);
    auto table = DomFactory::create_element(arena, TagNames::Table);
    table->set_attribute(Attr::Width, "100%");
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

    Hummingbird::Test::TestGraphicsContext context;
    Rect viewport{0, 0, 200, 200};
    render_root->layout(context, viewport);

    auto* table_render = dynamic_cast<RenderTable*>(render_root->get_children()[0].get());
    ASSERT_NE(table_render, nullptr);
    ASSERT_EQ(table_render->get_children().size(), 2u);

    auto* first_row = dynamic_cast<RenderTableRow*>(table_render->get_children()[0].get());
    auto* second_row = dynamic_cast<RenderTableRow*>(table_render->get_children()[1].get());
    ASSERT_NE(first_row, nullptr);
    ASSERT_NE(second_row, nullptr);

    auto* cell11_render = dynamic_cast<RenderTableCell*>(first_row->get_children()[0].get());
    auto* cell12_render = dynamic_cast<RenderTableCell*>(first_row->get_children()[1].get());
    auto* cell21_render = dynamic_cast<RenderTableCell*>(second_row->get_children()[0].get());
    auto* cell22_render = dynamic_cast<RenderTableCell*>(second_row->get_children()[1].get());
    ASSERT_NE(cell11_render, nullptr);
    ASSERT_NE(cell12_render, nullptr);
    ASSERT_NE(cell21_render, nullptr);
    ASSERT_NE(cell22_render, nullptr);

    EXPECT_FLOAT_EQ(table_render->get_rect().width, 200.0f);
    EXPECT_FLOAT_EQ(cell11_render->get_rect().width, 96.0f);
    EXPECT_FLOAT_EQ(cell12_render->get_rect().width, 104.0f);
    EXPECT_FLOAT_EQ(cell21_render->get_rect().width, 96.0f);
    EXPECT_FLOAT_EQ(cell22_render->get_rect().width, 104.0f);
}

TEST(TableLayoutTest, ColspanExpandsColumnWidths) {
    Hummingbird::Core::ArenaAllocator arena(4096);
    auto body = DomFactory::create_element(arena, TagNames::Body);
    auto table = DomFactory::create_element(arena, TagNames::Table);
    auto row1 = DomFactory::create_element(arena, TagNames::Tr);
    auto cell11 = DomFactory::create_element(arena, TagNames::Td);
    cell11->append_child(DomFactory::create_text(arena, "A"));
    auto cell12 = DomFactory::create_element(arena, TagNames::Td);
    cell12->append_child(DomFactory::create_text(arena, "BB"));
    auto cell13 = DomFactory::create_element(arena, TagNames::Td);
    cell13->append_child(DomFactory::create_text(arena, "CCC"));
    row1->append_child(std::move(cell11));
    row1->append_child(std::move(cell12));
    row1->append_child(std::move(cell13));
    table->append_child(std::move(row1));

    auto row2 = DomFactory::create_element(arena, TagNames::Tr);
    auto cell21 = DomFactory::create_element(arena, TagNames::Td);
    cell21->set_attribute(Attr::ColSpan, "3");
    cell21->append_child(DomFactory::create_text(arena, "WIDE TEXT"));
    row2->append_child(std::move(cell21));
    table->append_child(std::move(row2));
    body->append_child(std::move(table));

    Stylesheet sheet;
    StyleEngine engine;
    engine.apply(sheet, body.get());

    TreeBuilder builder;
    auto render_root = builder.build(body.get());
    ASSERT_NE(render_root, nullptr);

    Hummingbird::Test::TestGraphicsContext context;
    Rect viewport{0, 0, 400, 200};
    render_root->layout(context, viewport);

    auto* table_render = dynamic_cast<RenderTable*>(render_root->get_children()[0].get());
    ASSERT_NE(table_render, nullptr);
    auto* first_row = dynamic_cast<RenderTableRow*>(table_render->get_children()[0].get());
    auto* second_row = dynamic_cast<RenderTableRow*>(table_render->get_children()[1].get());
    ASSERT_NE(first_row, nullptr);
    ASSERT_NE(second_row, nullptr);

    auto* cell11_render = dynamic_cast<RenderTableCell*>(first_row->get_children()[0].get());
    auto* cell12_render = dynamic_cast<RenderTableCell*>(first_row->get_children()[1].get());
    auto* cell13_render = dynamic_cast<RenderTableCell*>(first_row->get_children()[2].get());
    auto* cell21_render = dynamic_cast<RenderTableCell*>(second_row->get_children()[0].get());
    ASSERT_NE(cell11_render, nullptr);
    ASSERT_NE(cell12_render, nullptr);
    ASSERT_NE(cell13_render, nullptr);
    ASSERT_NE(cell21_render, nullptr);

    EXPECT_NEAR(cell11_render->get_rect().width, 17.333334f, 0.001f);
    EXPECT_NEAR(cell12_render->get_rect().width, 25.333334f, 0.001f);
    EXPECT_NEAR(cell13_render->get_rect().width, 33.333332f, 0.001f);
    EXPECT_FLOAT_EQ(cell21_render->get_rect().width, 76.0f);
}

TEST(TableLayoutTest, AlignDoesNotInflateIntrinsicWidths) {
    Hummingbird::Core::ArenaAllocator arena(4096);
    auto body = DomFactory::create_element(arena, TagNames::Body);
    auto table = DomFactory::create_element(arena, TagNames::Table);
    auto row = DomFactory::create_element(arena, TagNames::Tr);
    auto cell1 = DomFactory::create_element(arena, TagNames::Td);
    cell1->set_attribute(Attr::Align, "center");
    cell1->append_child(DomFactory::create_text(arena, "HELLO"));
    auto cell2 = DomFactory::create_element(arena, TagNames::Td);
    cell2->append_child(DomFactory::create_text(arena, "B"));
    row->append_child(std::move(cell1));
    row->append_child(std::move(cell2));
    table->append_child(std::move(row));
    body->append_child(std::move(table));

    Stylesheet sheet;
    StyleEngine engine;
    engine.apply(sheet, body.get());

    TreeBuilder builder;
    auto render_root = builder.build(body.get());
    ASSERT_NE(render_root, nullptr);

    Hummingbird::Test::TestGraphicsContext context;
    Rect viewport{0, 0, 400, 200};
    render_root->layout(context, viewport);

    auto* table_render = dynamic_cast<RenderTable*>(render_root->get_children()[0].get());
    ASSERT_NE(table_render, nullptr);
    auto* row_render = dynamic_cast<RenderTableRow*>(table_render->get_children()[0].get());
    ASSERT_NE(row_render, nullptr);
    ASSERT_EQ(row_render->get_children().size(), 2u);

    auto* cell1_render = dynamic_cast<RenderTableCell*>(row_render->get_children()[0].get());
    auto* cell2_render = dynamic_cast<RenderTableCell*>(row_render->get_children()[1].get());
    ASSERT_NE(cell1_render, nullptr);
    ASSERT_NE(cell2_render, nullptr);

    EXPECT_FLOAT_EQ(table_render->get_rect().width, 56.0f);
    EXPECT_FLOAT_EQ(cell1_render->get_rect().width, 44.0f);
    EXPECT_FLOAT_EQ(cell2_render->get_rect().width, 12.0f);
}

TEST(TableLayoutTest, AlignsBlockChildrenInCells) {
    Hummingbird::Core::ArenaAllocator arena(4096);
    auto body = DomFactory::create_element(arena, TagNames::Body);
    auto table = DomFactory::create_element(arena, TagNames::Table);
    table->set_attribute(Attr::Width, "100");
    auto row = DomFactory::create_element(arena, TagNames::Tr);
    auto cell = DomFactory::create_element(arena, TagNames::Td);
    cell->set_attribute(Attr::Align, "right");
    auto block = DomFactory::create_element(arena, TagNames::Div);
    block->set_attribute(Attr::Width, "20");
    block->append_child(DomFactory::create_text(arena, "X"));
    cell->append_child(std::move(block));
    row->append_child(std::move(cell));
    table->append_child(std::move(row));
    body->append_child(std::move(table));

    Stylesheet sheet;
    StyleEngine engine;
    engine.apply(sheet, body.get());

    TreeBuilder builder;
    auto render_root = builder.build(body.get());
    ASSERT_NE(render_root, nullptr);

    Hummingbird::Test::TestGraphicsContext context;
    Rect viewport{0, 0, 200, 200};
    render_root->layout(context, viewport);

    auto* table_render = dynamic_cast<RenderTable*>(render_root->get_children()[0].get());
    ASSERT_NE(table_render, nullptr);
    auto* row_render = dynamic_cast<RenderTableRow*>(table_render->get_children()[0].get());
    ASSERT_NE(row_render, nullptr);
    auto* cell_render = dynamic_cast<RenderTableCell*>(row_render->get_children()[0].get());
    ASSERT_NE(cell_render, nullptr);
    ASSERT_FALSE(cell_render->get_children().empty());

    EXPECT_FLOAT_EQ(cell_render->get_rect().width, 100.0f);
    const auto& block_rect = cell_render->get_children()[0]->get_rect();
    EXPECT_FLOAT_EQ(block_rect.width, 20.0f);
    EXPECT_FLOAT_EQ(block_rect.x, 78.0f);
}
