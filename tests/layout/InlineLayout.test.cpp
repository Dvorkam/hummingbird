#include <gtest/gtest.h>

#include "core/ArenaAllocator.h"
#include "core/dom/DomFactory.h"
#include "core/dom/Element.h"
#include "core/dom/Text.h"
#include "core/platform_api/IImageDecoder.h"
#include "html/HtmlAttributeNames.h"
#include "layout/TreeBuilder.h"
#include "layout/replaced/RenderImage.h"
#include "layout/replaced/RenderSvg.h"
#include "style/compute/StyleEngine.h"
#include "style/parser/CssParser.h"
#include "test_utils/TestGraphicsContext.h"

using namespace Hummingbird::Layout;
using namespace Hummingbird::DOM;
using namespace Hummingbird::Css;
namespace Attr = Hummingbird::Html::AttributeNames;
using Hummingbird::ImageBitmap;
using Hummingbird::PixelFormat;

namespace {
const RenderObject* find_child_by_tag(const RenderObject& parent, std::string_view tag) {
    for (const auto& child : parent.get_children()) {
        auto* element = dynamic_cast<const Element*>(child->get_dom_node());
        if (element && element->get_tag_name() == tag) {
            return child.get();
        }
    }
    return nullptr;
}
}  // namespace

TEST(InlineLayoutTest, LaysOutInlineFlowOnSingleLine) {
    Hummingbird::Core::ArenaAllocator arena(4096);
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

    Hummingbird::Test::TestGraphicsContext context;
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
    Hummingbird::Core::ArenaAllocator arena(4096);
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

    Hummingbird::Test::TestGraphicsContext context;
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
    Hummingbird::Core::ArenaAllocator arena(4096);
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

    Hummingbird::Test::TestGraphicsContext context;
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
    Hummingbird::Core::ArenaAllocator arena(4096);
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

    Hummingbird::Test::TestGraphicsContext context;
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
    Hummingbird::Core::ArenaAllocator arena(4096);
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

    Hummingbird::Test::TestGraphicsContext context;
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
    Hummingbird::Core::ArenaAllocator arena(4096);
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

    Hummingbird::Test::TestGraphicsContext context;
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
    Hummingbird::Core::ArenaAllocator arena(4096);
    auto body = DomFactory::create_element(arena, "body");
    auto p = DomFactory::create_element(arena, "p");
    p->append_child(DomFactory::create_text(arena, "Hi"));
    auto img = DomFactory::create_element(arena, "img");
    img->set_attribute(Attr::Width, "64");
    img->set_attribute(Attr::Height, "32");
    p->append_child(std::move(img));
    p->append_child(DomFactory::create_text(arena, "!"));
    body->append_child(std::move(p));

    Stylesheet sheet;
    StyleEngine engine;
    engine.apply(sheet, body.get());

    TreeBuilder builder;
    auto render_root = builder.build(body.get());
    ASSERT_NE(render_root, nullptr);

    Hummingbird::Test::TestGraphicsContext context;
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
    EXPECT_GT(text_rect.y, image_rect.y);
}

TEST(InlineLayoutTest, InlineSvgUsesAttributeSizeAndFlows) {
    Hummingbird::Core::ArenaAllocator arena(4096);
    auto body = DomFactory::create_element(arena, "body");
    auto p = DomFactory::create_element(arena, "p");
    p->append_child(DomFactory::create_text(arena, "Hi"));
    auto svg = DomFactory::create_element(arena, "svg");
    svg->set_attribute(Attr::Width, "48");
    svg->set_attribute(Attr::Height, "24");
    p->append_child(std::move(svg));
    p->append_child(DomFactory::create_text(arena, "!"));
    body->append_child(std::move(p));

    Stylesheet sheet;
    StyleEngine engine;
    engine.apply(sheet, body.get());

    TreeBuilder builder;
    auto render_root = builder.build(body.get());
    ASSERT_NE(render_root, nullptr);

    Hummingbird::Test::TestGraphicsContext context;
    Rect viewport{0, 0, 400, 200};
    render_root->layout(context, viewport);

    const auto& para = render_root->get_children()[0];
    ASSERT_EQ(para->get_children().size(), 3u);
    const auto& text_rect = para->get_children()[0]->get_rect();
    auto* svg_box = dynamic_cast<RenderSvg*>(para->get_children()[1].get());
    ASSERT_NE(svg_box, nullptr);
    const auto& svg_rect = svg_box->get_rect();
    const auto& bang_rect = para->get_children()[2]->get_rect();

    EXPECT_FLOAT_EQ(svg_rect.width, 48.0f);
    EXPECT_FLOAT_EQ(svg_rect.height, 24.0f);
    EXPECT_FLOAT_EQ(svg_rect.x, text_rect.x + text_rect.width);
    EXPECT_FLOAT_EQ(bang_rect.x, svg_rect.x + svg_rect.width);
    EXPECT_GT(text_rect.y, svg_rect.y);
}

TEST(InlineLayoutTest, InlineImageDefaultsToPlaceholderSize) {
    Hummingbird::Core::ArenaAllocator arena(4096);
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

    Hummingbird::Test::TestGraphicsContext context;
    Rect viewport{0, 0, 500, 400};
    render_root->layout(context, viewport);

    const auto& para = render_root->get_children()[0];
    ASSERT_EQ(para->get_children().size(), 1u);
    const auto& image_rect = para->get_children()[0]->get_rect();

    EXPECT_FLOAT_EQ(image_rect.width, 300.0f);
    EXPECT_FLOAT_EQ(image_rect.height, 150.0f);
}

TEST(InlineLayoutTest, InlineImageUsesIntrinsicSizeWhenAvailable) {
    Hummingbird::Core::ArenaAllocator arena(4096);
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

    auto& para = render_root->get_children()[0];
    ASSERT_EQ(para->get_children().size(), 1u);
    auto* image = dynamic_cast<RenderImage*>(para->get_children()[0].get());
    ASSERT_NE(image, nullptr);

    ImageBitmap bitmap;
    bitmap.width = 40;
    bitmap.height = 20;
    bitmap.stride = 160;
    bitmap.format = PixelFormat::PRGB32;
    bitmap.pixels.resize(static_cast<size_t>(bitmap.stride) * bitmap.height);

    // The element names its image; the context is what turns that name back into
    // pixels, exactly as the engine does at paint time.
    const Hummingbird::ResourceRef ref{1, 1};
    image->set_image(ref);

    Hummingbird::Test::TestGraphicsContext context;
    Hummingbird::Test::StubImageResolver resolver(ref, &bitmap);
    context.set_resource_resolver(&resolver);
    Rect viewport{0, 0, 400, 200};
    render_root->layout(context, viewport);

    const auto& image_rect = image->get_rect();
    EXPECT_FLOAT_EQ(image_rect.width, 40.0f);
    EXPECT_FLOAT_EQ(image_rect.height, 20.0f);
}

TEST(InlineLayoutTest, AlignAttributeCentersInlineText) {
    Hummingbird::Core::ArenaAllocator arena(4096);
    auto body = DomFactory::create_element(arena, "body");
    auto p = DomFactory::create_element(arena, "p");
    p->set_attribute(Attr::Align, "center");
    p->append_child(DomFactory::create_text(arena, "Hi"));
    body->append_child(std::move(p));

    Stylesheet sheet;
    StyleEngine engine;
    engine.apply(sheet, body.get());

    TreeBuilder builder;
    auto render_root = builder.build(body.get());
    ASSERT_NE(render_root, nullptr);

    Hummingbird::Test::TestGraphicsContext context;
    Rect viewport{0, 0, 200, 200};
    render_root->layout(context, viewport);

    const auto& para = render_root->get_children()[0];
    ASSERT_EQ(para->get_children().size(), 1u);
    const auto& text_rect = para->get_children()[0]->get_rect();

    EXPECT_FLOAT_EQ(text_rect.width, 16.0f);
    EXPECT_FLOAT_EQ(text_rect.x, 92.0f);
}

TEST(InlineLayoutTest, TextAlignCssCentersInlineText) {
    Hummingbird::Core::ArenaAllocator arena(4096);
    auto body = DomFactory::create_element(arena, "body");
    auto p = DomFactory::create_element(arena, "p");
    p->append_child(DomFactory::create_text(arena, "Hi"));
    body->append_child(std::move(p));

    std::string css = "p { text-align: center; }";
    Parser parser(css);
    auto sheet = parser.parse();

    StyleEngine engine;
    engine.apply(sheet, body.get());

    TreeBuilder builder;
    auto render_root = builder.build(body.get());
    ASSERT_NE(render_root, nullptr);

    Hummingbird::Test::TestGraphicsContext context;
    Rect viewport{0, 0, 200, 200};
    render_root->layout(context, viewport);

    const auto& para = render_root->get_children()[0];
    ASSERT_EQ(para->get_children().size(), 1u);
    const auto& text_rect = para->get_children()[0]->get_rect();

    EXPECT_FLOAT_EQ(text_rect.width, 16.0f);
    EXPECT_FLOAT_EQ(text_rect.x, 92.0f);
}

TEST(InlineLayoutTest, NoWrapAttributeKeepsSingleLine) {
    Hummingbird::Core::ArenaAllocator arena(4096);
    auto body = DomFactory::create_element(arena, "body");
    auto p = DomFactory::create_element(arena, "p");
    p->set_attribute(Attr::NoWrap, "");
    p->append_child(DomFactory::create_text(arena, "Hello Hello"));
    body->append_child(std::move(p));

    Stylesheet sheet;
    StyleEngine engine;
    engine.apply(sheet, body.get());

    TreeBuilder builder;
    auto render_root = builder.build(body.get());
    ASSERT_NE(render_root, nullptr);

    Hummingbird::Test::TestGraphicsContext context;
    Rect viewport{0, 0, 60, 200};
    render_root->layout(context, viewport);

    const auto& para = render_root->get_children()[0];
    ASSERT_EQ(para->get_children().size(), 1u);
    const auto& text_rect = para->get_children()[0]->get_rect();

    EXPECT_FLOAT_EQ(text_rect.height, 16.0f);
    EXPECT_GT(text_rect.width, viewport.width);
}

TEST(InlineLayoutTest, NoWrapCssKeepsSingleLine) {
    Hummingbird::Core::ArenaAllocator arena(4096);
    auto body = DomFactory::create_element(arena, "body");
    auto p = DomFactory::create_element(arena, "p");
    p->append_child(DomFactory::create_text(arena, "Hello Hello"));
    body->append_child(std::move(p));

    std::string css = "p { white-space: nowrap; }";
    Parser parser(css);
    auto sheet = parser.parse();

    StyleEngine engine;
    engine.apply(sheet, body.get());

    TreeBuilder builder;
    auto render_root = builder.build(body.get());
    ASSERT_NE(render_root, nullptr);

    Hummingbird::Test::TestGraphicsContext context;
    Rect viewport{0, 0, 60, 200};
    render_root->layout(context, viewport);

    const auto& para = render_root->get_children()[0];
    ASSERT_EQ(para->get_children().size(), 1u);
    const auto& text_rect = para->get_children()[0]->get_rect();

    EXPECT_FLOAT_EQ(text_rect.height, 16.0f);
    EXPECT_GT(text_rect.width, viewport.width);
}

TEST(InlineLayoutTest, AlignsInlineTextOnBaseline) {
    Hummingbird::Core::ArenaAllocator arena(4096);
    auto body = DomFactory::create_element(arena, "body");
    auto p = DomFactory::create_element(arena, "p");
    auto big = DomFactory::create_element(arena, "span");
    big->set_attribute(Attr::Class, "big");
    big->append_child(DomFactory::create_text(arena, "Big"));
    auto small = DomFactory::create_element(arena, "span");
    small->set_attribute(Attr::Class, "small");
    small->append_child(DomFactory::create_text(arena, "small"));
    p->append_child(std::move(big));
    p->append_child(DomFactory::create_text(arena, " "));
    p->append_child(std::move(small));
    body->append_child(std::move(p));

    std::string css = ".big { font-size: 24px; } .small { font-size: 12px; }";
    Parser parser(css);
    auto sheet = parser.parse();

    StyleEngine engine;
    engine.apply(sheet, body.get());

    TreeBuilder builder;
    auto render_root = builder.build(body.get());
    ASSERT_NE(render_root, nullptr);

    Hummingbird::Test::TestGraphicsContext context;
    Rect viewport{0, 0, 400, 200};
    render_root->layout(context, viewport);

    const auto& para = render_root->get_children()[0];
    ASSERT_EQ(para->get_children().size(), 2u);

    const auto& big_span = para->get_children()[0];
    ASSERT_EQ(big_span->get_children().size(), 1u);
    const auto& big_rect = big_span->get_rect();

    const auto& small_span = para->get_children()[1];
    ASSERT_EQ(small_span->get_children().size(), 1u);
    const auto& small_rect = small_span->get_rect();

    EXPECT_GT(small_rect.y, big_rect.y);
}

TEST(InlineLayoutTest, AlignsFormControlsOnSharedBaseline) {
    Hummingbird::Core::ArenaAllocator arena(4096);
    auto body = DomFactory::create_element(arena, "body");
    auto form = DomFactory::create_element(arena, "form");
    form->append_child(DomFactory::create_element(arena, "input"));
    auto button = DomFactory::create_element(arena, "button");
    button->append_child(DomFactory::create_text(arena, "Search"));
    form->append_child(std::move(button));
    body->append_child(std::move(form));

    Stylesheet sheet;
    StyleEngine engine;
    engine.apply(sheet, body.get());

    TreeBuilder builder;
    auto render_root = builder.build(body.get());
    ASSERT_NE(render_root, nullptr);

    Hummingbird::Test::TestGraphicsContext context;
    Rect viewport{0, 0, 400, 200};
    render_root->layout(context, viewport);

    const auto& form_box = render_root->get_children()[0];
    const auto* input_box = find_child_by_tag(*form_box, "input");
    const auto* button_box = find_child_by_tag(*form_box, "button");
    ASSERT_NE(input_box, nullptr);
    ASSERT_NE(button_box, nullptr);

    EXPECT_FLOAT_EQ(input_box->get_rect().y, button_box->get_rect().y);
}

TEST(InlineLayoutTest, AlignsInlineBlockOnTextBaseline) {
    Hummingbird::Core::ArenaAllocator arena(4096);
    auto body = DomFactory::create_element(arena, "body");
    auto p = DomFactory::create_element(arena, "p");

    auto big = DomFactory::create_element(arena, "span");
    big->set_attribute(Attr::Class, "big");
    big->append_child(DomFactory::create_text(arena, "Big"));
    p->append_child(std::move(big));
    p->append_child(DomFactory::create_text(arena, " "));

    auto ib = DomFactory::create_element(arena, "span");
    ib->set_attribute(Attr::Class, "ib");
    ib->append_child(DomFactory::create_text(arena, "ib"));
    p->append_child(std::move(ib));

    body->append_child(std::move(p));

    std::string css =
        ".big { font-size: 24px; } .ib { display: inline-block; padding: 2px; border-width: 1px; "
        "border-style: solid; }";
    Parser parser(css);
    auto sheet = parser.parse();

    StyleEngine engine;
    engine.apply(sheet, body.get());

    TreeBuilder builder;
    auto render_root = builder.build(body.get());
    ASSERT_NE(render_root, nullptr);

    Hummingbird::Test::TestGraphicsContext context;
    Rect viewport{0, 0, 400, 200};
    render_root->layout(context, viewport);

    const auto& para = render_root->get_children()[0];
    ASSERT_EQ(para->get_children().size(), 2u);
    const auto& big_span = para->get_children()[0];
    const auto& ib_span = para->get_children()[1];

    // Inline-block should share the line baseline with surrounding text, which
    // means it sits below larger text runs rather than drifting upward.
    EXPECT_GT(ib_span->get_rect().y, big_span->get_rect().y);
}

TEST(InlineLayoutTest, VerticalAlignTopLiftsInlineTextRun) {
    Hummingbird::Core::ArenaAllocator arena(4096);
    auto body = DomFactory::create_element(arena, "body");
    auto p = DomFactory::create_element(arena, "p");

    auto top = DomFactory::create_element(arena, "span");
    top->set_attribute(Attr::Class, "top");
    top->append_child(DomFactory::create_text(arena, "Top"));
    p->append_child(std::move(top));
    p->append_child(DomFactory::create_text(arena, " "));

    auto baseline = DomFactory::create_element(arena, "span");
    baseline->set_attribute(Attr::Class, "base");
    baseline->append_child(DomFactory::create_text(arena, "base"));
    p->append_child(std::move(baseline));

    body->append_child(std::move(p));

    std::string css =
        ".top { vertical-align: top; line-height: 24px; } "
        ".base { line-height: 10px; }";
    Parser parser(css);
    auto sheet = parser.parse();

    StyleEngine engine;
    engine.apply(sheet, body.get());

    TreeBuilder builder;
    auto render_root = builder.build(body.get());
    ASSERT_NE(render_root, nullptr);

    Hummingbird::Test::TestGraphicsContext context;
    Rect viewport{0, 0, 400, 200};
    render_root->layout(context, viewport);

    const auto& para = render_root->get_children()[0];
    ASSERT_EQ(para->get_children().size(), 2u);
    const auto& top_span = para->get_children()[0];
    const auto& base_span = para->get_children()[1];

    EXPECT_LT(top_span->get_rect().y, base_span->get_rect().y);
}
