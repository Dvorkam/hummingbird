#include <gtest/gtest.h>

#include <string>
#include <string_view>

#include "core/ArenaAllocator.h"
#include "core/dom/DomFactory.h"
#include "core/dom/Element.h"
#include "layout/TreeBuilder.h"
#include "style/compute/StyleEngine.h"
#include "style/parser/CssParser.h"
#include "test_utils/TestGraphicsContext.h"

using namespace Hummingbird::Css;
using namespace Hummingbird::DOM;
using namespace Hummingbird::Layout;

namespace {

RenderObject* find_by_id(RenderObject* node, std::string_view id) {
    if (!node) return nullptr;
    if (auto* element = dynamic_cast<const Element*>(node->get_dom_node())) {
        if (const auto* attr = element->find_attribute("id")) {
            if (*attr == id) return node;
        }
    }
    for (const auto& child : node->get_children()) {
        if (auto* found = find_by_id(child.get(), id)) return found;
    }
    return nullptr;
}

struct GridFixture {
    Hummingbird::Core::ArenaAllocator arena{16384};
    Hummingbird::Core::ArenaPtr<Element> body;
    std::unique_ptr<RenderObject> render_root;
    Hummingbird::Test::TestGraphicsContext context;

    // <body><div id="c"> <div class="gi" id="i0"/> ... </div></body>
    void build(const std::string& css, int item_count) {
        body = DomFactory::create_element(arena, "body");
        auto container = DomFactory::create_element(arena, "div");
        container->set_attribute("id", "c");
        for (int i = 0; i < item_count; ++i) {
            auto item = DomFactory::create_element(arena, "div");
            item->set_attribute("id", "i" + std::to_string(i));
            item->set_attribute("class", "gi");
            container->append_child(std::move(item));
        }
        body->append_child(std::move(container));

        std::string full_css = "body, div { margin: 0; padding: 0; } .gi { height: 20px; } " + css;
        Parser parser(full_css);
        auto sheet = parser.parse();
        StyleEngine engine;
        engine.apply(sheet, body.get());

        TreeBuilder builder;
        render_root = builder.build(body.get());
        ASSERT_NE(render_root, nullptr);
        Rect viewport{0, 0, 800, 600};
        render_root->layout(context, viewport);
    }

    Rect rect(std::string_view id) {
        auto* node = find_by_id(render_root.get(), id);
        return node ? node->get_rect() : Rect{-1, -1, -1, -1};
    }
};

}  // namespace

TEST(GridLayoutTest, ThreeEqualFrColumns) {
    GridFixture fixture;
    fixture.build("#c { display: grid; grid-template-columns: repeat(3, 1fr); width: 300px; }", 3);
    EXPECT_FLOAT_EQ(fixture.rect("i0").x, 0.0f);
    EXPECT_FLOAT_EQ(fixture.rect("i0").width, 100.0f);
    EXPECT_FLOAT_EQ(fixture.rect("i1").x, 100.0f);
    EXPECT_FLOAT_EQ(fixture.rect("i2").x, 200.0f);
    EXPECT_FLOAT_EQ(fixture.rect("i2").width, 100.0f);
    // All three sit on the first row.
    EXPECT_FLOAT_EQ(fixture.rect("i0").y, 0.0f);
    EXPECT_FLOAT_EQ(fixture.rect("i2").y, 0.0f);
}

TEST(GridLayoutTest, FixedTrackPlusFrFillsRemainder) {
    GridFixture fixture;
    fixture.build("#c { display: grid; grid-template-columns: 100px 1fr; width: 300px; }", 2);
    EXPECT_FLOAT_EQ(fixture.rect("i0").x, 0.0f);
    EXPECT_FLOAT_EQ(fixture.rect("i0").width, 100.0f);
    EXPECT_FLOAT_EQ(fixture.rect("i1").x, 100.0f);
    EXPECT_FLOAT_EQ(fixture.rect("i1").width, 200.0f);
}

TEST(GridLayoutTest, ColumnGapReducesTrackSpace) {
    GridFixture fixture;
    fixture.build("#c { display: grid; grid-template-columns: repeat(2, 1fr); gap: 20px; width: 220px; }", 2);
    // 220 - 20 gap = 200 free, split 100/100.
    EXPECT_FLOAT_EQ(fixture.rect("i0").x, 0.0f);
    EXPECT_FLOAT_EQ(fixture.rect("i0").width, 100.0f);
    EXPECT_FLOAT_EQ(fixture.rect("i1").x, 120.0f);
    EXPECT_FLOAT_EQ(fixture.rect("i1").width, 100.0f);
}

TEST(GridLayoutTest, AutoPlacementWrapsToNextRow) {
    GridFixture fixture;
    fixture.build("#c { display: grid; grid-template-columns: repeat(3, 1fr); width: 300px; }", 4);
    // 4th item wraps to row 2 at column 0.
    EXPECT_FLOAT_EQ(fixture.rect("i3").x, 0.0f);
    EXPECT_FLOAT_EQ(fixture.rect("i3").y, 20.0f);  // below row 0 (height 20, no gap)
}

TEST(GridLayoutTest, RowGapOffsetsSecondRow) {
    GridFixture fixture;
    fixture.build("#c { display: grid; grid-template-columns: repeat(2, 1fr); row-gap: 10px; width: 200px; }", 3);
    // Row 0: i0,i1. Row 1: i2 at y = 20 (row height) + 10 (row gap).
    EXPECT_FLOAT_EQ(fixture.rect("i2").y, 30.0f);
}

TEST(GridLayoutTest, LineBasedSpanPlacement) {
    GridFixture fixture;
    fixture.build(
        "#c { display: grid; grid-template-columns: repeat(3, 1fr); width: 300px; } "
        "#i0 { grid-column: 1 / 3; }",
        2);
    // i0 spans columns 1-2 (200px); i1 auto-places into the free column 3.
    EXPECT_FLOAT_EQ(fixture.rect("i0").x, 0.0f);
    EXPECT_FLOAT_EQ(fixture.rect("i0").width, 200.0f);
    EXPECT_FLOAT_EQ(fixture.rect("i1").x, 200.0f);
    EXPECT_FLOAT_EQ(fixture.rect("i1").width, 100.0f);
}

TEST(GridLayoutTest, SpanKeywordPlacement) {
    GridFixture fixture;
    fixture.build(
        "#c { display: grid; grid-template-columns: repeat(3, 1fr); width: 300px; } "
        "#i0 { grid-column: span 2; }",
        2);
    EXPECT_FLOAT_EQ(fixture.rect("i0").width, 200.0f);
    EXPECT_FLOAT_EQ(fixture.rect("i1").x, 200.0f);
}

TEST(GridLayoutTest, ItemsStretchToCellByDefault) {
    GridFixture fixture;
    // No explicit item width -> stretch across the column.
    fixture.build("#c { display: grid; grid-template-columns: 1fr; width: 250px; }", 1);
    EXPECT_FLOAT_EQ(fixture.rect("i0").width, 250.0f);
}
