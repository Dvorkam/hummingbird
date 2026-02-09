#include <gtest/gtest.h>

#include "core/ArenaAllocator.h"
#include "core/dom/DomFactory.h"
#include "core/dom/Element.h"
#include "core/dom/Text.h"
#include "layout/TreeBuilder.h"
#include "layout/block/BlockBox.h"
#include "style/compute/StyleEngine.h"
#include "style/parser/CssParser.h"
#include "test_utils/TestGraphicsContext.h"

using namespace Hummingbird::Layout;
using namespace Hummingbird::DOM;
using namespace Hummingbird::Css;

TEST(LayoutStyleIntegrationTest, AppliesMarginPaddingAndWidth) {
    // DOM: <body><p>Hello</p><p>World</p></body>
    Hummingbird::Core::ArenaAllocator arena(4096);
    auto dom_root = DomFactory::create_element(arena, "body");
    auto p1 = DomFactory::create_element(arena, "p");
    p1->append_child(DomFactory::create_text(arena, "Hello"));
    auto p2 = DomFactory::create_element(arena, "p");
    p2->append_child(DomFactory::create_text(arena, "World"));
    dom_root->append_child(std::move(p1));
    dom_root->append_child(std::move(p2));

    // CSS
    std::string css = R"(
        body { padding: 5px; }
        p { margin: 3px; padding: 2px; }
    )";
    Parser parser(css);
    auto sheet = parser.parse();
    StyleEngine engine;
    engine.apply(sheet, dom_root.get());

    TreeBuilder builder;
    auto render_root = builder.build(dom_root.get());
    ASSERT_NE(render_root, nullptr);

    Hummingbird::Test::TestGraphicsContext context;
    Rect viewport{0, 0, 800, 600};
    render_root->layout(context, viewport);

    const auto& children = render_root->get_children();
    ASSERT_EQ(children.size(), 2u);

    const auto& rect1 = children[0]->get_rect();
    EXPECT_FLOAT_EQ(rect1.x, 8);        // padding 5 + margin 3
    EXPECT_FLOAT_EQ(rect1.y, 8);        // padding 5 + margin 3
    EXPECT_FLOAT_EQ(rect1.width, 784);  // 800 - padding*2 - margin*2
    EXPECT_FLOAT_EQ(rect1.height, 20);  // child height (16) + padding*2

    const auto& rect2 = children[1]->get_rect();
    EXPECT_FLOAT_EQ(rect2.x, 8);
    EXPECT_FLOAT_EQ(rect2.y, 34);  // previous y (8) + height 20 + margin bottom 3 + margin top 3
    EXPECT_FLOAT_EQ(rect2.width, 784);
    EXPECT_FLOAT_EQ(rect2.height, 20);

    // Check inner text boxes for padding offsets.
    const auto& text_rect1 = children[0]->get_children()[0]->get_rect();
    EXPECT_FLOAT_EQ(text_rect1.x, 2);       // padding-left
    EXPECT_FLOAT_EQ(text_rect1.y, 2);       // padding-top
    EXPECT_FLOAT_EQ(text_rect1.width, 40);  // 5 chars * 8 width
    EXPECT_FLOAT_EQ(text_rect1.height, 16);

    EXPECT_FLOAT_EQ(render_root->get_rect().height, 62);  // padding top 5 + margins/paddings + children
}

TEST(LayoutStyleIntegrationTest, IncludesBorderInInlineBoxSizing) {
    // DOM: <body><p><span>Hi</span></p></body>
    Hummingbird::Core::ArenaAllocator arena(4096);
    auto dom_root = DomFactory::create_element(arena, "body");
    auto p = DomFactory::create_element(arena, "p");
    auto span = DomFactory::create_element(arena, "span");
    span->append_child(DomFactory::create_text(arena, "Hi"));
    p->append_child(std::move(span));
    dom_root->append_child(std::move(p));

    std::string css = R"(
        span { border-width: 2px; border-style: solid; padding: 3px; }
    )";
    Parser parser(css);
    auto sheet = parser.parse();
    StyleEngine engine;
    engine.apply(sheet, dom_root.get());

    TreeBuilder builder;
    auto render_root = builder.build(dom_root.get());
    ASSERT_NE(render_root, nullptr);

    Hummingbird::Test::TestGraphicsContext context;
    Rect viewport{0, 0, 800, 600};
    render_root->layout(context, viewport);

    const auto& body_children = render_root->get_children();
    ASSERT_EQ(body_children.size(), 1u);
    const auto& p_children = body_children[0]->get_children();
    ASSERT_EQ(p_children.size(), 1u);
    const auto& span_render = p_children[0];

    const auto& rect = span_render->get_rect();
    // Text "Hi" is 2 chars -> 16px width; padding 3px each side, border 2px each side.
    EXPECT_FLOAT_EQ(rect.width, 16.0f + 2.0f * (3.0f + 2.0f));
    EXPECT_FLOAT_EQ(rect.height, 16.0f + 2.0f * (3.0f + 2.0f));
}

TEST(LayoutStyleIntegrationTest, HonorsBorderBoxSizingOnBlocks) {
    Hummingbird::Core::ArenaAllocator arena(4096);
    auto dom_root = DomFactory::create_element(arena, "body");
    auto div = DomFactory::create_element(arena, "div");
    div->append_child(DomFactory::create_text(arena, "Hello"));
    dom_root->append_child(std::move(div));

    std::string css = R"(
        div { width: 100px; padding: 10px; border-width: 5px; border-style: solid; box-sizing: border-box; }
    )";
    Parser parser(css);
    auto sheet = parser.parse();
    StyleEngine engine;
    engine.apply(sheet, dom_root.get());

    TreeBuilder builder;
    auto render_root = builder.build(dom_root.get());
    ASSERT_NE(render_root, nullptr);

    Hummingbird::Test::TestGraphicsContext context;
    Rect viewport{0, 0, 800, 600};
    render_root->layout(context, viewport);

    const auto& children = render_root->get_children();
    ASSERT_EQ(children.size(), 1u);
    const auto& rect = children[0]->get_rect();
    EXPECT_FLOAT_EQ(rect.width, 100.0f);
}

TEST(LayoutStyleIntegrationTest, HonorsMinMaxSizes) {
    Hummingbird::Core::ArenaAllocator arena(4096);
    auto dom_root = DomFactory::create_element(arena, "body");
    auto div = DomFactory::create_element(arena, "div");
    div->append_child(DomFactory::create_text(arena, "Hi"));
    dom_root->append_child(std::move(div));

    std::string css = R"(
        div { width: 50px; min-width: 120px; max-width: 140px; min-height: 30px; max-height: 40px; }
    )";
    Parser parser(css);
    auto sheet = parser.parse();
    StyleEngine engine;
    engine.apply(sheet, dom_root.get());

    TreeBuilder builder;
    auto render_root = builder.build(dom_root.get());
    ASSERT_NE(render_root, nullptr);

    Hummingbird::Test::TestGraphicsContext context;
    Rect viewport{0, 0, 800, 600};
    render_root->layout(context, viewport);

    const auto& children = render_root->get_children();
    ASSERT_EQ(children.size(), 1u);
    const auto& rect = children[0]->get_rect();
    EXPECT_FLOAT_EQ(rect.width, 120.0f);
    EXPECT_GE(rect.height, 30.0f);
    EXPECT_LE(rect.height, 40.0f);
}

TEST(LayoutStyleIntegrationTest, LaysOutInlineBlockInFlow) {
    // DOM: <body><p><span>A</span><span>B</span></p></body>
    Hummingbird::Core::ArenaAllocator arena(4096);
    auto dom_root = DomFactory::create_element(arena, "body");
    auto p = DomFactory::create_element(arena, "p");
    auto span1 = DomFactory::create_element(arena, "span");
    span1->append_child(DomFactory::create_text(arena, "A"));
    auto span2 = DomFactory::create_element(arena, "span");
    span2->append_child(DomFactory::create_text(arena, "B"));
    p->append_child(std::move(span1));
    p->append_child(std::move(span2));
    dom_root->append_child(std::move(p));

    std::string css = R"(
        span { display: inline-block; border-width: 1px; border-style: solid; padding: 2px; }
    )";
    Parser parser(css);
    auto sheet = parser.parse();
    StyleEngine engine;
    engine.apply(sheet, dom_root.get());

    TreeBuilder builder;
    auto render_root = builder.build(dom_root.get());
    ASSERT_NE(render_root, nullptr);

    Hummingbird::Test::TestGraphicsContext context;
    Rect viewport{0, 0, 800, 600};
    render_root->layout(context, viewport);

    const auto& para = render_root->get_children()[0];
    ASSERT_EQ(para->get_children().size(), 2u);
    const auto& first = para->get_children()[0]->get_rect();
    const auto& second = para->get_children()[1]->get_rect();

    EXPECT_FLOAT_EQ(first.y, second.y);
    EXPECT_GT(second.x, first.x + first.width - 0.1f);
    EXPECT_FLOAT_EQ(first.width, 8.0f + 2.0f * (2.0f + 1.0f));
    EXPECT_FLOAT_EQ(second.width, 8.0f + 2.0f * (2.0f + 1.0f));
}

TEST(LayoutStyleIntegrationTest, AppliesInputDefaultSizing) {
    Hummingbird::Core::ArenaAllocator arena(4096);
    auto dom_root = DomFactory::create_element(arena, "body");
    auto input = DomFactory::create_element(arena, "input");
    dom_root->append_child(std::move(input));

    Parser parser("");
    auto sheet = parser.parse();
    StyleEngine engine;
    engine.apply(sheet, dom_root.get());

    TreeBuilder builder;
    auto render_root = builder.build(dom_root.get());
    ASSERT_NE(render_root, nullptr);

    Hummingbird::Test::TestGraphicsContext context;
    Rect viewport{0, 0, 800, 600};
    render_root->layout(context, viewport);

    const auto& children = render_root->get_children();
    ASSERT_EQ(children.size(), 1u);
    const auto& rect = children[0]->get_rect();
    EXPECT_FLOAT_EQ(rect.width, 180.0f + 2.0f * (8.0f + 1.0f));
    EXPECT_FLOAT_EQ(rect.height, 24.0f + 2.0f * (4.0f + 1.0f));
}

TEST(LayoutStyleIntegrationTest, ResolvesPercentWidthsAgainstContainingBlock) {
    Hummingbird::Core::ArenaAllocator arena(4096);
    auto dom_root = DomFactory::create_element(arena, "body");
    auto parent = DomFactory::create_element(arena, "div");
    parent->set_attribute("class", "parent");
    auto child = DomFactory::create_element(arena, "div");
    child->set_attribute("class", "child");
    parent->append_child(std::move(child));
    dom_root->append_child(std::move(parent));

    std::string css = R"(
        body { margin: 0; padding: 0; }
        .parent { width: 400px; margin: 0; padding: 0; }
        .child { width: 70%; margin: 0; padding: 0; }
    )";
    Parser parser(css);
    auto sheet = parser.parse();
    StyleEngine engine;
    engine.apply(sheet, dom_root.get());

    TreeBuilder builder;
    auto render_root = builder.build(dom_root.get());
    ASSERT_NE(render_root, nullptr);

    Hummingbird::Test::TestGraphicsContext context;
    Rect viewport{0, 0, 800, 600};
    render_root->layout(context, viewport);

    const auto& children = render_root->get_children();
    ASSERT_EQ(children.size(), 1u);
    const auto& parent_rect = children[0]->get_rect();
    ASSERT_EQ(children[0]->get_children().size(), 1u);
    const auto& child_rect = children[0]->get_children()[0]->get_rect();

    EXPECT_FLOAT_EQ(parent_rect.width, 400.0f);
    EXPECT_FLOAT_EQ(child_rect.width, 280.0f);
}

TEST(LayoutStyleIntegrationTest, InputKeepsMinimumContentBoxUnderBorderBoxConstraints) {
    Hummingbird::Core::ArenaAllocator arena(4096);
    auto dom_root = DomFactory::create_element(arena, "body");
    auto input = DomFactory::create_element(arena, "input");
    input->set_attribute("type", "text");
    dom_root->append_child(std::move(input));

    std::string css = R"(
        input {
            width: 20px;
            height: 10px;
            box-sizing: border-box;
            padding: 5px 7px;
            border: 1px solid #666;
        }
    )";
    Parser parser(css);
    auto sheet = parser.parse();
    StyleEngine engine;
    engine.apply(sheet, dom_root.get());

    TreeBuilder builder;
    auto render_root = builder.build(dom_root.get());
    ASSERT_NE(render_root, nullptr);

    Hummingbird::Test::TestGraphicsContext context;
    Rect viewport{0, 0, 800, 600};
    render_root->layout(context, viewport);

    const auto& children = render_root->get_children();
    ASSERT_EQ(children.size(), 1u);
    const auto& rect = children[0]->get_rect();

    // 8+8 horizontal and 6+6 vertical insets + minimum content box (8x12).
    EXPECT_FLOAT_EQ(rect.width, 24.0f);
    EXPECT_FLOAT_EQ(rect.height, 24.0f);
}
