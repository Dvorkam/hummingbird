#include <gtest/gtest.h>

#include "TestGraphicsContext.h"
#include "core/ArenaAllocator.h"
#include "core/dom/DomFactory.h"
#include "core/dom/Element.h"
#include "core/dom/Text.h"
#include "layout/TreeBuilder.h"
#include "style/CssParser.h"
#include "style/StyleEngine.h"

using namespace Hummingbird::Layout;
using namespace Hummingbird::DOM;
using namespace Hummingbird::Css;

TEST(InlineLayoutTest, LaysOutInlineFlowOnSingleLine) {
    ArenaAllocator arena(4096);
    auto body = DomFactory::create_element(arena, "body");
    auto p = DomFactory::create_element(arena, "p");
    p->append_child(DomFactory::create_text(arena, "Hello "));
    auto strong = DomFactory::create_element(arena, "strong");
    strong->append_child(DomFactory::create_text(arena, "World"));
    p->append_child(std::move(strong));
    p->append_child(DomFactory::create_text(arena, "!"));
    body->append_child(std::move(p));

    Stylesheet sheet;
    StyleEngine engine;
    engine.apply(sheet, body.get());

    TreeBuilder builder;
    auto render_root = builder.build(body.get());
    ASSERT_NE(render_root, nullptr);
    ASSERT_EQ(render_root->get_children().size(), 1u);

    TestGraphicsContext context;
    Rect viewport{0, 0, 300, 200};
    render_root->layout(context, viewport);

    const auto& para = render_root->get_children()[0];
    ASSERT_EQ(para->get_children().size(), 3u);
    const auto& first = para->get_children()[0]->get_rect();
    const auto& second = para->get_children()[1]->get_rect();
    const auto& third = para->get_children()[2]->get_rect();

    EXPECT_FLOAT_EQ(first.x, 0.0f);
    EXPECT_FLOAT_EQ(second.x, first.x + first.width);
    EXPECT_FLOAT_EQ(third.x, second.x + second.width);
    EXPECT_FLOAT_EQ(first.y, second.y);
    EXPECT_FLOAT_EQ(second.y, third.y);
}

TEST(InlineLayoutTest, IndentsListsPerUserAgentDefaults) {
    ArenaAllocator arena(4096);
    auto body = DomFactory::create_element(arena, "body");
    auto ul = DomFactory::create_element(arena, "ul");
    ul->append_child(DomFactory::create_element(arena, "li"));
    auto ol = DomFactory::create_element(arena, "ol");
    ol->append_child(DomFactory::create_element(arena, "li"));
    body->append_child(std::move(ul));
    body->append_child(std::move(ol));

    Stylesheet sheet;
    StyleEngine engine;
    engine.apply(sheet, body.get());

    TreeBuilder builder;
    auto render_root = builder.build(body.get());
    ASSERT_NE(render_root, nullptr);
    ASSERT_EQ(render_root->get_children().size(), 2u);

    TestGraphicsContext context;
    Rect viewport{0, 0, 400, 300};
    render_root->layout(context, viewport);

    const auto& ul_box = render_root->get_children()[0];
    ASSERT_EQ(ul_box->get_children().size(), 1u);
    EXPECT_FLOAT_EQ(ul_box->get_children()[0]->get_rect().x, 20.0f);

    const auto& ol_box = render_root->get_children()[1];
    ASSERT_EQ(ol_box->get_children().size(), 1u);
    EXPECT_FLOAT_EQ(ol_box->get_children()[0]->get_rect().x, 20.0f);
}

TEST(InlineLayoutTest, GreedyWrapsInlineTextWithinWidth) {
    ArenaAllocator arena(4096);
    auto body = DomFactory::create_element(arena, "body");
    auto p = DomFactory::create_element(arena, "p");
    // "HelloHello" (10 chars) at 8px each = 80px > 60px available forces wrap.
    p->append_child(DomFactory::create_text(arena, "Hello Hello"));
    body->append_child(std::move(p));

    Stylesheet sheet;
    StyleEngine engine;
    engine.apply(sheet, body.get());

    TreeBuilder builder;
    auto render_root = builder.build(body.get());
    ASSERT_NE(render_root, nullptr);

    TestGraphicsContext context;
    Rect viewport{0, 0, 60, 200};
    render_root->layout(context, viewport);

    const auto& para = render_root->get_children()[0];
    ASSERT_EQ(para->get_children().size(), 1u);
    const auto& text_rect = para->get_children()[0]->get_rect();

    // Wrapped: width should not exceed available width and height should include two lines (2 * 16px).
    EXPECT_LE(text_rect.width, viewport.width);
    EXPECT_GE(text_rect.height, 32.0f);
}

TEST(InlineLayoutTest, PreservesSpacesAroundInlineElements) {
    ArenaAllocator arena(4096);
    auto body = DomFactory::create_element(arena, "body");
    auto p = DomFactory::create_element(arena, "p");
    p->append_child(DomFactory::create_text(arena, "Hello "));
    auto a = DomFactory::create_element(arena, "a");
    a->append_child(DomFactory::create_text(arena, "link"));
    p->append_child(std::move(a));
    p->append_child(DomFactory::create_text(arena, " world"));
    body->append_child(std::move(p));

    Stylesheet sheet;
    StyleEngine engine;
    engine.apply(sheet, body.get());

    TreeBuilder builder;
    auto render_root = builder.build(body.get());
    ASSERT_NE(render_root, nullptr);
    const auto& para = render_root->get_children()[0];
    ASSERT_EQ(para->get_children().size(), 3u);

    TestGraphicsContext context;
    Rect viewport{0, 0, 200, 200};
    render_root->layout(context, viewport);

    const auto& first = para->get_children()[0]->get_rect();
    const auto& link = para->get_children()[1]->get_rect();
    const auto& last = para->get_children()[2]->get_rect();

    // Expect the link to start after the first text width (includes trailing space)
    EXPECT_GT(link.x, first.x + 0.1f);
    // Expect trailing text to start after link width (space preserved)
    EXPECT_GT(last.x, link.x + link.width - 0.1f);
}

TEST(InlineLayoutTest, ContinuesInlineFlowAfterWrappedText) {
    ArenaAllocator arena(4096);
    auto body = DomFactory::create_element(arena, "body");
    auto p = DomFactory::create_element(arena, "p");
    p->append_child(DomFactory::create_text(arena, "Hello Hello Hello "));
    auto code = DomFactory::create_element(arena, "code");
    code->append_child(DomFactory::create_text(arena, "code"));
    p->append_child(std::move(code));
    body->append_child(std::move(p));

    Stylesheet sheet;
    StyleEngine engine;
    engine.apply(sheet, body.get());

    TreeBuilder builder;
    auto render_root = builder.build(body.get());
    ASSERT_NE(render_root, nullptr);

    TestGraphicsContext context;
    Rect viewport{0, 0, 120, 200};
    render_root->layout(context, viewport);

    const auto& para = render_root->get_children()[0];
    ASSERT_EQ(para->get_children().size(), 2u);
    const auto& text_rect = para->get_children()[0]->get_rect();
    const auto& code_rect = para->get_children()[1]->get_rect();

    // Text wraps to two lines (16px line height). Code should continue on the second line.
    EXPECT_FLOAT_EQ(code_rect.y, text_rect.y + 16.0f);
    EXPECT_GT(code_rect.x, 0.0f);
}

TEST(InlineLayoutTest, InlineBoxWithPaddingIsAtomic) {
    ArenaAllocator arena(4096);
    auto body = DomFactory::create_element(arena, "body");
    auto p = DomFactory::create_element(arena, "p");
    auto span = DomFactory::create_element(arena, "span");
    span->append_child(DomFactory::create_text(arena, "Hello"));
    p->append_child(std::move(span));
    body->append_child(std::move(p));

    std::string css = "span { padding: 2px; border-width: 1px; border-style: solid; }";
    Parser parser(css);
    auto sheet = parser.parse();

    StyleEngine engine;
    engine.apply(sheet, body.get());

    TreeBuilder builder;
    auto render_root = builder.build(body.get());
    ASSERT_NE(render_root, nullptr);

    TestGraphicsContext context;
    Rect viewport{0, 0, 300, 200};
    render_root->layout(context, viewport);

    const auto& para = render_root->get_children()[0];
    ASSERT_EQ(para->get_children().size(), 1u);
    const auto& span_box = para->get_children()[0];
    ASSERT_EQ(span_box->get_children().size(), 1u);
    const auto& span_rect = span_box->get_rect();
    const auto& text_rect = span_box->get_children()[0]->get_rect();

    EXPECT_GT(span_rect.width, text_rect.width);
    EXPECT_FLOAT_EQ(span_rect.width, text_rect.width + 6.0f);
}

TEST(InlineLayoutTest, InlineImageUsesAttributeSizeAndFlows) {
    ArenaAllocator arena(4096);
    auto body = DomFactory::create_element(arena, "body");
    auto p = DomFactory::create_element(arena, "p");
    p->append_child(DomFactory::create_text(arena, "Hi"));
    auto img = DomFactory::create_element(arena, "img");
    img->set_attribute("width", "64");
    img->set_attribute("height", "32");
    p->append_child(std::move(img));
    p->append_child(DomFactory::create_text(arena, "!"));
    body->append_child(std::move(p));

    Stylesheet sheet;
    StyleEngine engine;
    engine.apply(sheet, body.get());

    TreeBuilder builder;
    auto render_root = builder.build(body.get());
    ASSERT_NE(render_root, nullptr);

    TestGraphicsContext context;
    Rect viewport{0, 0, 400, 200};
    render_root->layout(context, viewport);

    const auto& para = render_root->get_children()[0];
    ASSERT_EQ(para->get_children().size(), 3u);
    const auto& text_rect = para->get_children()[0]->get_rect();
    const auto& image_rect = para->get_children()[1]->get_rect();
    const auto& bang_rect = para->get_children()[2]->get_rect();

    EXPECT_FLOAT_EQ(image_rect.width, 64.0f);
    EXPECT_FLOAT_EQ(image_rect.height, 32.0f);
    EXPECT_FLOAT_EQ(image_rect.x, text_rect.x + text_rect.width);
    EXPECT_FLOAT_EQ(bang_rect.x, image_rect.x + image_rect.width);
    EXPECT_FLOAT_EQ(text_rect.y, image_rect.y);
}

TEST(InlineLayoutTest, InlineImageDefaultsToPlaceholderSize) {
    ArenaAllocator arena(4096);
    auto body = DomFactory::create_element(arena, "body");
    auto p = DomFactory::create_element(arena, "p");
    p->append_child(DomFactory::create_element(arena, "img"));
    body->append_child(std::move(p));

    Stylesheet sheet;
    StyleEngine engine;
    engine.apply(sheet, body.get());

    TreeBuilder builder;
    auto render_root = builder.build(body.get());
    ASSERT_NE(render_root, nullptr);

    TestGraphicsContext context;
    Rect viewport{0, 0, 500, 400};
    render_root->layout(context, viewport);

    const auto& para = render_root->get_children()[0];
    ASSERT_EQ(para->get_children().size(), 1u);
    const auto& image_rect = para->get_children()[0]->get_rect();

    EXPECT_FLOAT_EQ(image_rect.width, 300.0f);
    EXPECT_FLOAT_EQ(image_rect.height, 150.0f);
}
