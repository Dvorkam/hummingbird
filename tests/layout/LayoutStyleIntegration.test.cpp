#include <gtest/gtest.h>

#include "TestGraphicsContext.h"
#include "core/ArenaAllocator.h"
#include "core/dom/Element.h"
#include "core/dom/Text.h"
#include "layout/BlockBox.h"
#include "layout/TreeBuilder.h"
#include "style/CssParser.h"
#include "style/StyleEngine.h"

using namespace Hummingbird::Layout;
using namespace Hummingbird::DOM;
using namespace Hummingbird::Css;

TEST(LayoutStyleIntegrationTest, AppliesMarginPaddingAndWidth) {
    // DOM: <body><p>Hello</p><p>World</p></body>
    ArenaAllocator arena(4096);
    auto dom_root = make_arena_ptr<Element>(arena, "body");
    auto p1 = make_arena_ptr<Element>(arena, "p");
    p1->append_child(make_arena_ptr<Text>(arena, "Hello"));
    auto p2 = make_arena_ptr<Element>(arena, "p");
    p2->append_child(make_arena_ptr<Text>(arena, "World"));
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

    TestGraphicsContext context;
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
    ArenaAllocator arena(4096);
    auto dom_root = make_arena_ptr<Element>(arena, "body");
    auto p = make_arena_ptr<Element>(arena, "p");
    auto span = make_arena_ptr<Element>(arena, "span");
    span->append_child(make_arena_ptr<Text>(arena, "Hi"));
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

    TestGraphicsContext context;
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

TEST(LayoutStyleIntegrationTest, LaysOutInlineBlockInFlow) {
    // DOM: <body><p><span>A</span><span>B</span></p></body>
    ArenaAllocator arena(4096);
    auto dom_root = make_arena_ptr<Element>(arena, "body");
    auto p = make_arena_ptr<Element>(arena, "p");
    auto span1 = make_arena_ptr<Element>(arena, "span");
    span1->append_child(make_arena_ptr<Text>(arena, "A"));
    auto span2 = make_arena_ptr<Element>(arena, "span");
    span2->append_child(make_arena_ptr<Text>(arena, "B"));
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

    TestGraphicsContext context;
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
