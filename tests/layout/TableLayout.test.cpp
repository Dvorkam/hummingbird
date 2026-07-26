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
#include "style/parser/CssParser.h"
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

TEST(TableLayoutTest, BlockChildDoesNotBalloonCellIntrinsicWidth) {
    // Regression for T-LAYOUT-TABLE-INTRINSIC-BLOCK-1: a table cell whose child is
    // display:block must be measured at its content width, not stretched to the
    // 100000px intrinsic-measurement box. Otherwise that column balloons and
    // pushes every later column off-screen (the Hacker News front-page failure:
    // `.votelinks a { display:block }` shoved story titles to x~100032).
    Hummingbird::Core::ArenaAllocator arena(8192);
    auto body = DomFactory::create_element(arena, TagNames::Body);
    auto table = DomFactory::create_element(arena, TagNames::Table);
    auto row = DomFactory::create_element(arena, TagNames::Tr);

    auto rank = DomFactory::create_element(arena, TagNames::Td);
    rank->append_child(DomFactory::create_text(arena, "1."));
    row->append_child(std::move(rank));

    // Middle cell: an <a class="vote"> (display:block via CSS) wrapping an empty div.
    auto votelinks = DomFactory::create_element(arena, TagNames::Td);
    auto vote_anchor = DomFactory::create_element(arena, "a");
    vote_anchor->set_attribute("class", "vote");
    vote_anchor->append_child(DomFactory::create_element(arena, "div"));
    votelinks->append_child(std::move(vote_anchor));
    row->append_child(std::move(votelinks));

    auto title = DomFactory::create_element(arena, TagNames::Td);
    title->append_child(DomFactory::create_text(arena, "A story title"));
    row->append_child(std::move(title));

    table->append_child(std::move(row));
    body->append_child(std::move(table));

    Parser css_parser(".vote { display: block; }");
    auto sheet = css_parser.parse();
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
    ASSERT_EQ(row_render->get_children().size(), 3u);
    auto* title_cell = dynamic_cast<RenderTableCell*>(row_render->get_children()[2].get());
    ASSERT_NE(title_cell, nullptr);

    // The title column must sit within the table, not ~100000px to the right.
    EXPECT_LT(title_cell->get_rect().x, 200.0f) << "title column pushed off-screen at x=" << title_cell->get_rect().x;
    // ...and the block-bearing column must not balloon the whole table.
    EXPECT_LT(table_render->get_rect().width, 1000.0f) << "table width ballooned to " << table_render->get_rect().width;
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

TEST(TableLayoutTest, NestedPercentWidthTableDoesNotBalloonOuterTable) {
    // HN's header: an outer table whose cell holds `<table width="100%">`. During
    // the outer cell's intrinsic measurement the inner table must act as auto
    // (shrink to content), not fill the ~100000px probe — otherwise the outer
    // table ballooned to ~67000px and pushed the logout link off-screen.
    Hummingbird::Core::ArenaAllocator arena(4096);
    auto body = DomFactory::create_element(arena, TagNames::Body);
    auto outer = DomFactory::create_element(arena, TagNames::Table);
    auto outer_row = DomFactory::create_element(arena, TagNames::Tr);
    auto outer_cell = DomFactory::create_element(arena, TagNames::Td);

    auto inner = DomFactory::create_element(arena, TagNames::Table);
    inner->set_attribute(Attr::Width, "100%");
    auto inner_row = DomFactory::create_element(arena, TagNames::Tr);
    auto inner_cell = DomFactory::create_element(arena, TagNames::Td);
    inner_cell->append_child(DomFactory::create_text(arena, "logout"));
    inner_row->append_child(std::move(inner_cell));
    inner->append_child(std::move(inner_row));
    outer_cell->append_child(std::move(inner));
    outer_row->append_child(std::move(outer_cell));
    outer->append_child(std::move(outer_row));
    body->append_child(std::move(outer));

    Stylesheet sheet;
    StyleEngine engine;
    engine.apply(sheet, body.get());

    TreeBuilder builder;
    auto render_root = builder.build(body.get());
    ASSERT_NE(render_root, nullptr);
    Hummingbird::Test::TestGraphicsContext context;
    Rect viewport{0, 0, 1024, 400};
    render_root->layout(context, viewport);

    auto* outer_render = dynamic_cast<RenderTable*>(render_root->get_children()[0].get());
    ASSERT_NE(outer_render, nullptr);
    EXPECT_LT(outer_render->get_rect().width, 1024.0f)
        << "outer table ballooned to " << outer_render->get_rect().width;
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

TEST(TableLayoutTest, RespectsFiftyFiftyCellWidthHintsOnPercentTable) {
    Hummingbird::Core::ArenaAllocator arena(4096);
    auto body = DomFactory::create_element(arena, TagNames::Body);
    auto table = DomFactory::create_element(arena, TagNames::Table);
    table->set_attribute(Attr::Width, "100%");

    auto row = DomFactory::create_element(arena, TagNames::Tr);
    auto first = DomFactory::create_element(arena, TagNames::Td);
    first->set_attribute(Attr::Width, "50%");
    first->append_child(DomFactory::create_text(arena, "A"));
    auto second = DomFactory::create_element(arena, TagNames::Td);
    second->set_attribute(Attr::Width, "50%");
    second->append_child(DomFactory::create_text(arena, "LONGER-CELL"));
    row->append_child(std::move(first));
    row->append_child(std::move(second));
    table->append_child(std::move(row));
    body->append_child(std::move(table));

    Stylesheet sheet;
    StyleEngine engine;
    engine.apply(sheet, body.get());

    TreeBuilder builder;
    auto render_root = builder.build(body.get());
    ASSERT_NE(render_root, nullptr);

    Hummingbird::Test::TestGraphicsContext context;
    Rect viewport{0, 0, 320, 200};
    render_root->layout(context, viewport);

    auto* table_render = dynamic_cast<RenderTable*>(render_root->get_children()[0].get());
    ASSERT_NE(table_render, nullptr);
    auto* row_render = dynamic_cast<RenderTableRow*>(table_render->get_children()[0].get());
    ASSERT_NE(row_render, nullptr);
    ASSERT_EQ(row_render->get_children().size(), 2u);

    auto* first_cell = dynamic_cast<RenderTableCell*>(row_render->get_children()[0].get());
    auto* second_cell = dynamic_cast<RenderTableCell*>(row_render->get_children()[1].get());
    ASSERT_NE(first_cell, nullptr);
    ASSERT_NE(second_cell, nullptr);

    EXPECT_FLOAT_EQ(table_render->get_rect().width, 320.0f);
    EXPECT_FLOAT_EQ(first_cell->get_rect().width, 160.0f);
    EXPECT_FLOAT_EQ(second_cell->get_rect().width, 160.0f);
}

TEST(TableLayoutTest, KeepsColumnsReadableWithMixedInlineContentAndPercentHints) {
    Hummingbird::Core::ArenaAllocator arena(8192);
    auto body = DomFactory::create_element(arena, TagNames::Body);
    auto table = DomFactory::create_element(arena, TagNames::Table);
    table->set_attribute(Attr::Width, "100%");

    auto row1 = DomFactory::create_element(arena, TagNames::Tr);
    auto row1c1 = DomFactory::create_element(arena, TagNames::Td);
    row1c1->set_attribute(Attr::Width, "50%");
    row1c1->append_child(DomFactory::create_text(arena, "Short"));
    auto row1c2 = DomFactory::create_element(arena, TagNames::Td);
    row1c2->set_attribute(Attr::Width, "50%");
    row1c2->append_child(DomFactory::create_text(arena, "Longer text sample"));
    row1->append_child(std::move(row1c1));
    row1->append_child(std::move(row1c2));
    table->append_child(std::move(row1));

    auto row2 = DomFactory::create_element(arena, TagNames::Tr);
    auto row2c1 = DomFactory::create_element(arena, TagNames::Td);
    auto block = DomFactory::create_element(arena, TagNames::Div);
    block->set_attribute(Attr::Width, "140");
    block->append_child(DomFactory::create_text(arena, "Inline+block"));
    row2c1->append_child(std::move(block));
    auto row2c2 = DomFactory::create_element(arena, TagNames::Td);
    row2c2->append_child(DomFactory::create_text(arena, "B"));
    row2->append_child(std::move(row2c1));
    row2->append_child(std::move(row2c2));
    table->append_child(std::move(row2));
    body->append_child(std::move(table));

    Stylesheet sheet;
    StyleEngine engine;
    engine.apply(sheet, body.get());

    TreeBuilder builder;
    auto render_root = builder.build(body.get());
    ASSERT_NE(render_root, nullptr);

    Hummingbird::Test::TestGraphicsContext context;
    Rect viewport{0, 0, 480, 240};
    render_root->layout(context, viewport);

    auto* table_render = dynamic_cast<RenderTable*>(render_root->get_children()[0].get());
    ASSERT_NE(table_render, nullptr);
    ASSERT_EQ(table_render->get_children().size(), 2u);
    auto* first_row = dynamic_cast<RenderTableRow*>(table_render->get_children()[0].get());
    ASSERT_NE(first_row, nullptr);
    ASSERT_EQ(first_row->get_children().size(), 2u);
    auto* first_cell = dynamic_cast<RenderTableCell*>(first_row->get_children()[0].get());
    auto* second_cell = dynamic_cast<RenderTableCell*>(first_row->get_children()[1].get());
    ASSERT_NE(first_cell, nullptr);
    ASSERT_NE(second_cell, nullptr);

    EXPECT_FLOAT_EQ(table_render->get_rect().width, 480.0f);
    EXPECT_GE(first_cell->get_rect().width, 240.0f);
    EXPECT_GE(second_cell->get_rect().width, 240.0f);
}

TEST(TableLayoutTest, NormalizesOvercommittedPercentHints) {
    Hummingbird::Core::ArenaAllocator arena(4096);
    auto body = DomFactory::create_element(arena, TagNames::Body);
    auto table = DomFactory::create_element(arena, TagNames::Table);
    table->set_attribute(Attr::Width, "100%");

    auto row = DomFactory::create_element(arena, TagNames::Tr);
    auto first = DomFactory::create_element(arena, TagNames::Td);
    first->set_attribute(Attr::Width, "70%");
    first->append_child(DomFactory::create_text(arena, "Short"));
    auto second = DomFactory::create_element(arena, TagNames::Td);
    second->set_attribute(Attr::Width, "70%");
    second->append_child(DomFactory::create_text(arena, "Also short"));
    row->append_child(std::move(first));
    row->append_child(std::move(second));
    table->append_child(std::move(row));
    body->append_child(std::move(table));

    Stylesheet sheet;
    StyleEngine engine;
    engine.apply(sheet, body.get());

    TreeBuilder builder;
    auto render_root = builder.build(body.get());
    ASSERT_NE(render_root, nullptr);

    Hummingbird::Test::TestGraphicsContext context;
    Rect viewport{0, 0, 300, 200};
    render_root->layout(context, viewport);

    auto* table_render = dynamic_cast<RenderTable*>(render_root->get_children()[0].get());
    ASSERT_NE(table_render, nullptr);
    auto* row_render = dynamic_cast<RenderTableRow*>(table_render->get_children()[0].get());
    ASSERT_NE(row_render, nullptr);
    ASSERT_EQ(row_render->get_children().size(), 2u);

    auto* first_cell = dynamic_cast<RenderTableCell*>(row_render->get_children()[0].get());
    auto* second_cell = dynamic_cast<RenderTableCell*>(row_render->get_children()[1].get());
    ASSERT_NE(first_cell, nullptr);
    ASSERT_NE(second_cell, nullptr);

    EXPECT_FLOAT_EQ(table_render->get_rect().width, 300.0f);
    EXPECT_NEAR(first_cell->get_rect().width, 150.0f, 0.01f);
    EXPECT_NEAR(second_cell->get_rect().width, 150.0f, 0.01f);
}

TEST(TableLayoutTest, RespectsAbsoluteCellWidthHintsOnPercentTable) {
    Hummingbird::Core::ArenaAllocator arena(4096);
    auto body = DomFactory::create_element(arena, TagNames::Body);
    auto table = DomFactory::create_element(arena, TagNames::Table);
    table->set_attribute(Attr::Width, "100%");

    auto row = DomFactory::create_element(arena, TagNames::Tr);
    auto first = DomFactory::create_element(arena, TagNames::Td);
    first->set_attribute(Attr::Width, "180");
    first->append_child(DomFactory::create_text(arena, "A"));
    auto second = DomFactory::create_element(arena, TagNames::Td);
    second->append_child(DomFactory::create_text(arena, "B"));
    row->append_child(std::move(first));
    row->append_child(std::move(second));
    table->append_child(std::move(row));
    body->append_child(std::move(table));

    Stylesheet sheet;
    StyleEngine engine;
    engine.apply(sheet, body.get());

    TreeBuilder builder;
    auto render_root = builder.build(body.get());
    ASSERT_NE(render_root, nullptr);

    Hummingbird::Test::TestGraphicsContext context;
    Rect viewport{0, 0, 300, 200};
    render_root->layout(context, viewport);

    auto* table_render = dynamic_cast<RenderTable*>(render_root->get_children()[0].get());
    ASSERT_NE(table_render, nullptr);
    auto* row_render = dynamic_cast<RenderTableRow*>(table_render->get_children()[0].get());
    ASSERT_NE(row_render, nullptr);
    ASSERT_EQ(row_render->get_children().size(), 2u);

    auto* first_cell = dynamic_cast<RenderTableCell*>(row_render->get_children()[0].get());
    auto* second_cell = dynamic_cast<RenderTableCell*>(row_render->get_children()[1].get());
    ASSERT_NE(first_cell, nullptr);
    ASSERT_NE(second_cell, nullptr);

    EXPECT_FLOAT_EQ(table_render->get_rect().width, 300.0f);
    EXPECT_GE(first_cell->get_rect().width, 180.0f);
    EXPECT_LE(first_cell->get_rect().width, 288.0f);
    EXPECT_GT(second_cell->get_rect().width, 0.0f);
    EXPECT_NEAR(second_cell->get_rect().x, first_cell->get_rect().width, 0.01f);
    EXPECT_LE(first_cell->get_rect().width + second_cell->get_rect().width, table_render->get_rect().width);
}

TEST(TableLayoutTest, KeepsMixedSectionRowsAlignedWithAbsoluteAndPercentHints) {
    Hummingbird::Core::ArenaAllocator arena(8192);
    auto body = DomFactory::create_element(arena, TagNames::Body);
    auto table = DomFactory::create_element(arena, TagNames::Table);
    table->set_attribute(Attr::Width, "100%");

    auto thead = DomFactory::create_element(arena, TagNames::Thead);
    auto head_row = DomFactory::create_element(arena, TagNames::Tr);
    auto head_first = DomFactory::create_element(arena, TagNames::Th);
    head_first->append_child(DomFactory::create_text(arena, "Name"));
    auto head_second = DomFactory::create_element(arena, TagNames::Th);
    head_second->append_child(DomFactory::create_text(arena, "Description"));
    head_row->append_child(std::move(head_first));
    head_row->append_child(std::move(head_second));
    thead->append_child(std::move(head_row));
    table->append_child(std::move(thead));

    auto tbody = DomFactory::create_element(arena, TagNames::Tbody);
    auto row1 = DomFactory::create_element(arena, TagNames::Tr);
    auto row1_first = DomFactory::create_element(arena, TagNames::Td);
    row1_first->set_attribute(Attr::Width, "180");
    row1_first->append_child(DomFactory::create_text(arena, "Fixed column"));
    auto row1_second = DomFactory::create_element(arena, TagNames::Td);
    row1_second->set_attribute(Attr::Width, "70%");
    row1_second->append_child(DomFactory::create_text(arena, "Longer content that should stay readable."));
    row1->append_child(std::move(row1_first));
    row1->append_child(std::move(row1_second));
    tbody->append_child(std::move(row1));

    auto row2 = DomFactory::create_element(arena, TagNames::Tr);
    auto row2_first = DomFactory::create_element(arena, TagNames::Td);
    row2_first->append_child(DomFactory::create_text(arena, "Short"));
    auto row2_second = DomFactory::create_element(arena, TagNames::Td);
    row2_second->append_child(DomFactory::create_text(arena, "Second row still aligns to same split."));
    row2->append_child(std::move(row2_first));
    row2->append_child(std::move(row2_second));
    tbody->append_child(std::move(row2));
    table->append_child(std::move(tbody));
    body->append_child(std::move(table));

    Stylesheet sheet;
    StyleEngine engine;
    engine.apply(sheet, body.get());

    TreeBuilder builder;
    auto render_root = builder.build(body.get());
    ASSERT_NE(render_root, nullptr);

    Hummingbird::Test::TestGraphicsContext context;
    Rect viewport{0, 0, 520, 260};
    render_root->layout(context, viewport);

    auto* table_render = dynamic_cast<RenderTable*>(render_root->get_children()[0].get());
    ASSERT_NE(table_render, nullptr);
    ASSERT_EQ(table_render->get_children().size(), 2u);

    auto* section_head = dynamic_cast<RenderTableSection*>(table_render->get_children()[0].get());
    auto* section_body = dynamic_cast<RenderTableSection*>(table_render->get_children()[1].get());
    ASSERT_NE(section_head, nullptr);
    ASSERT_NE(section_body, nullptr);
    ASSERT_EQ(section_head->get_children().size(), 1u);
    ASSERT_EQ(section_body->get_children().size(), 2u);

    auto* head_row_render = dynamic_cast<RenderTableRow*>(section_head->get_children()[0].get());
    auto* body_row_render = dynamic_cast<RenderTableRow*>(section_body->get_children()[0].get());
    ASSERT_NE(head_row_render, nullptr);
    ASSERT_NE(body_row_render, nullptr);
    ASSERT_EQ(head_row_render->get_children().size(), 2u);
    ASSERT_EQ(body_row_render->get_children().size(), 2u);

    auto* head_second_cell = dynamic_cast<RenderTableCell*>(head_row_render->get_children()[1].get());
    auto* body_first_cell = dynamic_cast<RenderTableCell*>(body_row_render->get_children()[0].get());
    auto* body_second_cell = dynamic_cast<RenderTableCell*>(body_row_render->get_children()[1].get());
    ASSERT_NE(head_second_cell, nullptr);
    ASSERT_NE(body_first_cell, nullptr);
    ASSERT_NE(body_second_cell, nullptr);

    EXPECT_GE(table_render->get_rect().width, 520.0f);
    EXPECT_GE(body_first_cell->get_rect().width, 180.0f);
    EXPECT_NEAR(body_second_cell->get_rect().x, body_first_cell->get_rect().width, 0.01f);
    EXPECT_NEAR(head_second_cell->get_rect().x, body_second_cell->get_rect().x, 0.01f);
    EXPECT_LE(body_first_cell->get_rect().width + body_second_cell->get_rect().width, table_render->get_rect().width);
}
