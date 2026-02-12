#include "layout/table/TableColumnLayout.h"

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

namespace {
RenderTable* first_table(RenderObject* render_root) {
    if (!render_root || render_root->get_children().empty()) {
        return nullptr;
    }
    return dynamic_cast<RenderTable*>(render_root->get_children()[0].get());
}
}  // namespace

TEST(TableColumnLayoutTest, NormalizesOvercommittedPercentHintsInPlanner) {
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
    auto* table_render = first_table(render_root.get());
    ASSERT_NE(table_render, nullptr);

    auto plan = compute_table_column_layout(*table_render, context, 300.0f);
    ASSERT_EQ(plan.column_widths.size(), 2u);
    EXPECT_NEAR(plan.content_width, 300.0f, 0.01f);
    EXPECT_NEAR(plan.column_widths[0], 150.0f, 0.01f);
    EXPECT_NEAR(plan.column_widths[1], 150.0f, 0.01f);
}

TEST(TableColumnLayoutTest, RespectsAbsoluteHintInPlanner) {
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
    auto* table_render = first_table(render_root.get());
    ASSERT_NE(table_render, nullptr);

    auto plan = compute_table_column_layout(*table_render, context, 300.0f);
    ASSERT_EQ(plan.column_widths.size(), 2u);
    EXPECT_NEAR(plan.content_width, 300.0f, 0.01f);
    EXPECT_GE(plan.column_widths[0], 180.0f);
    EXPECT_GT(plan.column_widths[1], 0.0f);
    EXPECT_LE(plan.column_widths[0] + plan.column_widths[1], 300.01f);
}
