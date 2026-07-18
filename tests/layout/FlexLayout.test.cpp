#include <gtest/gtest.h>

#include <string_view>

#include "core/ArenaAllocator.h"
#include "core/dom/DomFactory.h"
#include "core/dom/Element.h"
#include "core/dom/Text.h"
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
            if (*attr == id) {
                return node;
            }
        }
    }
    for (const auto& child : node->get_children()) {
        if (auto* found = find_by_id(child.get(), id)) {
            return found;
        }
    }
    return nullptr;
}

struct FlexFixture {
    Hummingbird::Core::ArenaAllocator arena{8192};
    Hummingbird::Core::ArenaPtr<Element> body;
    std::unique_ptr<RenderObject> render_root;
    Hummingbird::Test::TestGraphicsContext context;

    // Builds <body><div id="c"><div id="a"/><div id="b"/>[<div id="d"/>]</div></body>,
    // applies the given CSS, and lays out at an 800x600 viewport.
    void build(const std::string& css, bool third_item = false) {
        body = DomFactory::create_element(arena, "body");
        auto container = DomFactory::create_element(arena, "div");
        container->set_attribute("id", "c");
        auto a = DomFactory::create_element(arena, "div");
        a->set_attribute("id", "a");
        auto b = DomFactory::create_element(arena, "div");
        b->set_attribute("id", "b");
        container->append_child(std::move(a));
        container->append_child(std::move(b));
        if (third_item) {
            auto d = DomFactory::create_element(arena, "div");
            d->set_attribute("id", "d");
            container->append_child(std::move(d));
        }
        body->append_child(std::move(container));

        std::string full_css = "body, div { margin: 0; padding: 0; } " + css;
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
};

}  // namespace

TEST(FlexLayoutTest, RowPlacesItemsSideBySide) {
    FlexFixture fixture;
    fixture.build(R"(
        #c { display: flex; width: 300px; }
        #a { width: 50px; height: 20px; }
        #b { width: 70px; height: 30px; }
    )");

    auto* a = find_by_id(fixture.render_root.get(), "a");
    auto* b = find_by_id(fixture.render_root.get(), "b");
    auto* c = find_by_id(fixture.render_root.get(), "c");
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    ASSERT_NE(c, nullptr);

    EXPECT_FLOAT_EQ(a->get_rect().x, 0.0f);
    EXPECT_FLOAT_EQ(a->get_rect().y, 0.0f);
    EXPECT_FLOAT_EQ(a->get_rect().width, 50.0f);
    EXPECT_FLOAT_EQ(a->get_rect().height, 20.0f);
    EXPECT_FLOAT_EQ(b->get_rect().x, 50.0f);
    EXPECT_FLOAT_EQ(b->get_rect().y, 0.0f);
    EXPECT_FLOAT_EQ(b->get_rect().width, 70.0f);
    // Container hugs the tallest item.
    EXPECT_FLOAT_EQ(c->get_rect().height, 30.0f);
}

TEST(FlexLayoutTest, FlexGrowDistributesFreeSpaceProportionally) {
    FlexFixture fixture;
    fixture.build(R"(
        #c { display: flex; width: 300px; }
        #a { flex: 1; height: 10px; }
        #b { flex: 2; height: 10px; }
    )");

    auto* a = find_by_id(fixture.render_root.get(), "a");
    auto* b = find_by_id(fixture.render_root.get(), "b");
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);

    EXPECT_NEAR(a->get_rect().width, 100.0f, 0.5f);
    EXPECT_NEAR(b->get_rect().width, 200.0f, 0.5f);
    EXPECT_NEAR(b->get_rect().x, 100.0f, 0.5f);
}

TEST(FlexLayoutTest, FlexShrinkResolvesOverflowProportionally) {
    FlexFixture fixture;
    fixture.build(R"(
        #c { display: flex; width: 300px; }
        #a { width: 200px; height: 10px; }
        #b { width: 200px; height: 10px; }
    )");

    auto* a = find_by_id(fixture.render_root.get(), "a");
    auto* b = find_by_id(fixture.render_root.get(), "b");
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);

    EXPECT_NEAR(a->get_rect().width, 150.0f, 0.5f);
    EXPECT_NEAR(b->get_rect().width, 150.0f, 0.5f);
    EXPECT_NEAR(b->get_rect().x, 150.0f, 0.5f);
}

TEST(FlexLayoutTest, JustifyContentCenterAndSpaceBetween) {
    FlexFixture centered;
    centered.build(R"(
        #c { display: flex; width: 300px; justify-content: center; }
        #a { width: 50px; height: 10px; }
        #b { width: 50px; height: 10px; }
    )");
    auto* a = find_by_id(centered.render_root.get(), "a");
    auto* b = find_by_id(centered.render_root.get(), "b");
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    EXPECT_NEAR(a->get_rect().x, 100.0f, 0.5f);
    EXPECT_NEAR(b->get_rect().x, 150.0f, 0.5f);

    FlexFixture spaced;
    spaced.build(R"(
        #c { display: flex; width: 300px; justify-content: space-between; }
        #a { width: 50px; height: 10px; }
        #b { width: 50px; height: 10px; }
    )");
    a = find_by_id(spaced.render_root.get(), "a");
    b = find_by_id(spaced.render_root.get(), "b");
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    EXPECT_NEAR(a->get_rect().x, 0.0f, 0.5f);
    EXPECT_NEAR(b->get_rect().x, 250.0f, 0.5f);
}

TEST(FlexLayoutTest, AlignItemsCenterInDefiniteHeightContainer) {
    FlexFixture fixture;
    fixture.build(R"(
        #c { display: flex; width: 300px; height: 100px; align-items: center; }
        #a { width: 50px; height: 20px; }
        #b { width: 50px; height: 40px; }
    )");

    auto* a = find_by_id(fixture.render_root.get(), "a");
    auto* b = find_by_id(fixture.render_root.get(), "b");
    auto* c = find_by_id(fixture.render_root.get(), "c");
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    ASSERT_NE(c, nullptr);

    EXPECT_NEAR(a->get_rect().y, 40.0f, 0.5f);
    EXPECT_NEAR(b->get_rect().y, 30.0f, 0.5f);
    EXPECT_FLOAT_EQ(c->get_rect().height, 100.0f);
}

TEST(FlexLayoutTest, ColumnStacksItemsAndStretchesWidth) {
    FlexFixture fixture;
    fixture.build(R"(
        #c { display: flex; flex-direction: column; width: 300px; }
        #a { height: 20px; }
        #b { height: 30px; }
    )");

    auto* a = find_by_id(fixture.render_root.get(), "a");
    auto* b = find_by_id(fixture.render_root.get(), "b");
    auto* c = find_by_id(fixture.render_root.get(), "c");
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    ASSERT_NE(c, nullptr);

    EXPECT_FLOAT_EQ(a->get_rect().y, 0.0f);
    EXPECT_FLOAT_EQ(a->get_rect().height, 20.0f);
    EXPECT_FLOAT_EQ(b->get_rect().y, 20.0f);
    EXPECT_FLOAT_EQ(b->get_rect().height, 30.0f);
    // Stretch: items fill the cross axis (container width).
    EXPECT_FLOAT_EQ(a->get_rect().width, 300.0f);
    EXPECT_FLOAT_EQ(b->get_rect().width, 300.0f);
    EXPECT_FLOAT_EQ(c->get_rect().height, 50.0f);
}

TEST(FlexLayoutTest, OrderControlsVisualPlacement) {
    FlexFixture fixture;
    fixture.build(R"(
        #c { display: flex; width: 300px; }
        #a { width: 50px; height: 10px; order: 2; }
        #b { width: 50px; height: 10px; order: 1; }
    )");

    auto* a = find_by_id(fixture.render_root.get(), "a");
    auto* b = find_by_id(fixture.render_root.get(), "b");
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);

    EXPECT_NEAR(b->get_rect().x, 0.0f, 0.5f);
    EXPECT_NEAR(a->get_rect().x, 50.0f, 0.5f);
}

TEST(FlexLayoutTest, RowReverseReversesVisualOrder) {
    FlexFixture fixture;
    fixture.build(R"(
        #c { display: flex; flex-direction: row-reverse; width: 300px; justify-content: flex-start; }
        #a { width: 50px; height: 10px; }
        #b { width: 50px; height: 10px; }
    )");

    auto* a = find_by_id(fixture.render_root.get(), "a");
    auto* b = find_by_id(fixture.render_root.get(), "b");
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);

    // Visual order flips: b first, then a.
    EXPECT_NEAR(b->get_rect().x, 0.0f, 0.5f);
    EXPECT_NEAR(a->get_rect().x, 50.0f, 0.5f);
}

TEST(FlexLayoutTest, FlexShorthandExpandsToGrowShrinkBasis) {
    FlexFixture fixture;
    fixture.build(R"(
        #c { display: flex; width: 300px; }
        #a { flex: 2; height: 10px; }
        #b { flex: 0 0 80px; height: 10px; }
    )");

    auto* a_node = find_by_id(fixture.render_root.get(), "a");
    auto* b_node = find_by_id(fixture.render_root.get(), "b");
    ASSERT_NE(a_node, nullptr);
    ASSERT_NE(b_node, nullptr);

    const auto* a_style = a_node->get_computed_style();
    const auto* b_style = b_node->get_computed_style();
    ASSERT_NE(a_style, nullptr);
    ASSERT_NE(b_style, nullptr);

    EXPECT_FLOAT_EQ(a_style->flex_grow, 2.0f);
    EXPECT_FLOAT_EQ(a_style->flex_shrink, 1.0f);
    ASSERT_TRUE(a_style->flex_basis.has_value());
    EXPECT_FLOAT_EQ(a_style->flex_basis->px, 0.0f);

    EXPECT_FLOAT_EQ(b_style->flex_grow, 0.0f);
    EXPECT_FLOAT_EQ(b_style->flex_shrink, 0.0f);
    ASSERT_TRUE(b_style->flex_basis.has_value());
    EXPECT_FLOAT_EQ(b_style->flex_basis->px, 80.0f);

    // Layout consequence: a takes the remaining 220px, b keeps its 80px basis.
    EXPECT_NEAR(a_node->get_rect().width, 220.0f, 0.5f);
    EXPECT_NEAR(b_node->get_rect().width, 80.0f, 0.5f);
}

TEST(FlexLayoutTest, PercentBasisResolvesAgainstContainerMainSize) {
    FlexFixture fixture;
    fixture.build(R"(
        #c { display: flex; width: 400px; }
        #a { flex-basis: 25%; height: 10px; }
        #b { flex-basis: 50%; height: 10px; }
    )");

    auto* a = find_by_id(fixture.render_root.get(), "a");
    auto* b = find_by_id(fixture.render_root.get(), "b");
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);

    EXPECT_NEAR(a->get_rect().width, 100.0f, 0.5f);
    EXPECT_NEAR(b->get_rect().width, 200.0f, 0.5f);
    EXPECT_NEAR(b->get_rect().x, 100.0f, 0.5f);
}

TEST(FlexLayoutTest, FlexWrapMovesOverflowingItemToNextLine) {
    FlexFixture fixture;
    // a(60)+b(60)=120 fit in 130; d(60) overflows and wraps to a second line.
    fixture.build(R"(
        #c { display: flex; flex-wrap: wrap; width: 130px; }
        #a { width: 60px; height: 20px; flex-shrink: 0; }
        #b { width: 60px; height: 20px; flex-shrink: 0; }
        #d { width: 60px; height: 30px; flex-shrink: 0; }
    )",
                  /*third_item=*/true);

    auto* a = find_by_id(fixture.render_root.get(), "a");
    auto* b = find_by_id(fixture.render_root.get(), "b");
    auto* d = find_by_id(fixture.render_root.get(), "d");
    auto* c = find_by_id(fixture.render_root.get(), "c");
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    ASSERT_NE(d, nullptr);
    ASSERT_NE(c, nullptr);

    // Line 1: a and b side by side at the top.
    EXPECT_FLOAT_EQ(a->get_rect().y, 0.0f);
    EXPECT_FLOAT_EQ(b->get_rect().x, 60.0f);
    EXPECT_FLOAT_EQ(b->get_rect().y, 0.0f);
    // Line 2: d drops below line 1 (whose cross height is 20).
    EXPECT_FLOAT_EQ(d->get_rect().x, 0.0f);
    EXPECT_FLOAT_EQ(d->get_rect().y, 20.0f);
    // Container height spans both lines: 20 + 30.
    EXPECT_FLOAT_EQ(c->get_rect().height, 50.0f);
}

TEST(FlexLayoutTest, FlexWrapReverseStacksLinesFromOppositeSide) {
    FlexFixture fixture;
    fixture.build(R"(
        #c { display: flex; flex-wrap: wrap-reverse; width: 130px; }
        #a { width: 60px; height: 20px; flex-shrink: 0; }
        #b { width: 60px; height: 20px; flex-shrink: 0; }
        #d { width: 60px; height: 30px; flex-shrink: 0; }
    )",
                  /*third_item=*/true);

    auto* a = find_by_id(fixture.render_root.get(), "a");
    auto* d = find_by_id(fixture.render_root.get(), "d");
    auto* c = find_by_id(fixture.render_root.get(), "c");
    ASSERT_NE(a, nullptr);
    ASSERT_NE(d, nullptr);
    ASSERT_NE(c, nullptr);

    // wrap-reverse puts the last line (d) at the top and the first line (a,b) below.
    EXPECT_FLOAT_EQ(d->get_rect().y, 0.0f);
    EXPECT_FLOAT_EQ(a->get_rect().y, 30.0f);
    EXPECT_FLOAT_EQ(c->get_rect().height, 50.0f);
}

TEST(FlexLayoutTest, AlignItemsBaselineAlignsFirstLineBaselines) {
    FlexFixture fixture;
    // Same box height, very different font sizes -> different ascents. Baseline
    // alignment keeps the larger-text item at the top and pushes the smaller-text
    // item down so their text baselines line up.
    fixture.build(R"(
        #c { display: flex; align-items: baseline; width: 300px; }
        #a { width: 40px; height: 50px; font-size: 40px; }
        #b { width: 40px; height: 50px; font-size: 10px; }
    )");

    auto* a = find_by_id(fixture.render_root.get(), "a");
    auto* b = find_by_id(fixture.render_root.get(), "b");
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);

    EXPECT_NEAR(a->get_rect().y, 0.0f, 1.0f);
    // b is shifted down by roughly (ascent_a - ascent_b) ~ (32 - 8) = 24px.
    EXPECT_GT(b->get_rect().y, 10.0f);
    EXPECT_NEAR(b->get_rect().y, 24.0f, 3.0f);
}

TEST(FlexLayoutTest, CheckboxInputIsFixedSquareInFlexRow) {
    Hummingbird::Core::ArenaAllocator arena{8192};
    Hummingbird::Test::TestGraphicsContext context;

    auto body = DomFactory::create_element(arena, "body");
    auto li = DomFactory::create_element(arena, "li");
    li->set_attribute("id", "li");
    auto box = DomFactory::create_element(arena, "input");
    box->set_attribute("id", "box");
    box->set_attribute("type", "checkbox");
    auto label = DomFactory::create_element(arena, "span");
    label->set_attribute("id", "lbl");
    label->append_child(DomFactory::create_text(arena, "a task"));
    li->append_child(std::move(box));
    li->append_child(std::move(label));
    body->append_child(std::move(li));

    std::string css =
        "body { margin: 0; padding: 0; } "
        "li { display: flex; flex-direction: row; align-items: center; width: 590px; } "
        "#box { flex: none; margin: 0; } "
        "#lbl { flex: 1; margin: 0 12px; }";
    Parser parser(css);
    auto sheet = parser.parse();
    StyleEngine engine;
    engine.apply(sheet, body.get());

    TreeBuilder builder;
    auto root = builder.build(body.get());
    ASSERT_NE(root, nullptr);
    Rect viewport{0, 0, 800, 600};
    root->layout(context, viewport);

    // A checkbox must stay a fixed square, not stretch to a text-input rectangle
    // just because it sits in a flex row (the UA default gives it width==height).
    auto* box_box = find_by_id(root.get(), "box");
    ASSERT_NE(box_box, nullptr);
    const auto& r = box_box->get_rect();
    EXPECT_NEAR(r.width, 16.0f, 0.5f);
    EXPECT_NEAR(r.height, 16.0f, 0.5f);
}
