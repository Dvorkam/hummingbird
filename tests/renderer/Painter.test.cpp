#include "renderer/Painter.h"

#include <gtest/gtest.h>

#include <cmath>
#include <functional>

#include "core/dom/Element.h"
#include "core/platform_api/IGraphicsContext.h"
#include "core/platform_api/IImageDecoder.h"
#include "html/HtmlParser.h"
#include "html/HtmlTagNames.h"
#include "layout/TreeBuilder.h"
#include "layout/flow/TextBox.h"
#include "layout/paint/PaintUtils.h"
#include "layout/formatting/RenderListItem.h"
#include "style/compute/StyleEngine.h"
#include "style/parser/CssParser.h"

using Hummingbird::Color;
using Hummingbird::IGraphicsContext;
using Hummingbird::ImageBitmap;
using Hummingbird::TextMetrics;
using Hummingbird::TextStyle;
using Hummingbird::Layout::RenderObject;
using namespace Hummingbird::Css;

// Graphics context that records draw_text calls and provides deterministic metrics.
class RecordingGraphicsContext : public IGraphicsContext {
public:
    void set_viewport(const Hummingbird::Layout::Rect& viewport) override { viewport_ = viewport; }
    void clear(const Color&) override {}
    void present() override {}
    void fill_rect(const Hummingbird::Layout::Rect& rect, const Color&) override { fill_calls.push_back(rect); }
    void draw_image(const ImageBitmap& /*image*/, const Hummingbird::Layout::Rect& /*dest*/) override { ++image_calls; }

    TextMetrics measure_text(const std::string& text, const TextStyle&) override {
        TextMetrics metrics;
        metrics.width = static_cast<float>(text.size()) * 8.0f;
        metrics.height = 16.0f;
        metrics.ascent = 12.0f;
        metrics.descent = 4.0f;
        return metrics;
    }

    void draw_text(const std::string& text, float, float, const TextStyle&) override {
        ++draw_calls;
        last_text = text;
        drawn_texts.push_back(text);
        draw_alphas.push_back(current_alpha);
    }
    void set_global_alpha(float alpha) override { current_alpha = alpha; }

    int draw_calls = 0;
    int image_calls = 0;
    std::string last_text;
    std::vector<std::string> drawn_texts;
    std::vector<Hummingbird::Layout::Rect> fill_calls;
    std::vector<float> draw_alphas;
    float current_alpha = 1.0f;
    Hummingbird::Layout::Rect viewport_{0, 0, 0, 0};
};

class FontAwareGraphicsContext : public IGraphicsContext {
public:
    void set_viewport(const Hummingbird::Layout::Rect& viewport) override { viewport_ = viewport; }
    void clear(const Color&) override {}
    void present() override {}
    void fill_rect(const Hummingbird::Layout::Rect& rect, const Color&) override { fill_calls.push_back(rect); }
    void draw_image(const ImageBitmap& /*image*/, const Hummingbird::Layout::Rect& /*dest*/) override {}

    TextMetrics measure_text(const std::string& text, const TextStyle& style) override {
        const float font_size = style.font_size > 0.0f ? style.font_size : 16.0f;
        TextMetrics metrics;
        metrics.width = static_cast<float>(text.size()) * font_size * 0.5f;
        metrics.height = font_size;
        metrics.ascent = font_size * 0.8f;
        metrics.descent = font_size * 0.2f;
        metrics.underline_position = -metrics.descent * 0.5f;
        metrics.underline_thickness = 1.0f;
        return metrics;
    }

    void draw_text(const std::string&, float, float, const TextStyle&) override {}

    std::vector<Hummingbird::Layout::Rect> fill_calls;
    Hummingbird::Layout::Rect viewport_{0, 0, 0, 0};
};

TEST(PainterIntegrationTest, PaintsTextNodesFromParserOutput) {
    // Arrange: parse a simple HTML snippet.
    std::string_view html = "<html><body><p>First line</p><p>Second line</p></body></html>";
    Hummingbird::Core::ArenaAllocator arena(2048);
    Hummingbird::Html::Parser parser(arena, html);
    auto result = parser.parse();

    // Build render tree.
    Hummingbird::Layout::TreeBuilder builder;
    auto render_tree = builder.build(result.dom.get());
    ASSERT_NE(render_tree, nullptr);

    // Layout and paint with recording context.
    RecordingGraphicsContext context;
    Hummingbird::Layout::Rect viewport{0, 0, 800, 600};
    render_tree->layout(context, viewport);

    Hummingbird::Renderer::Painter painter;
    Hummingbird::Renderer::PaintOptions opts;
    painter.paint(*render_tree, context, opts);

    // Expect one draw call per tokenized run.
    EXPECT_EQ(context.draw_calls, 6);
    EXPECT_EQ(context.last_text, "line");
}

TEST(PainterDebugTest, DrawsOutlinesWhenDebugEnabled) {
    // DOM: <html><body><p>Text</p></body></html>
    std::string_view html = "<html><body><p>Text</p></body></html>";
    Hummingbird::Core::ArenaAllocator arena(2048);
    Hummingbird::Html::Parser parser(arena, html);
    auto result = parser.parse();

    Hummingbird::Layout::TreeBuilder builder;
    auto render_tree = builder.build(result.dom.get());
    ASSERT_NE(render_tree, nullptr);

    RecordingGraphicsContext context;
    Hummingbird::Layout::Rect viewport{0, 0, 200, 200};
    render_tree->layout(context, viewport);

    Hummingbird::Renderer::Painter painter;
    Hummingbird::Renderer::PaintOptions opts;
    opts.debug_outlines = true;
    painter.paint(*render_tree, context, opts);

    // Expect at least one fill_rect call for debug outlines.
    EXPECT_FALSE(context.fill_calls.empty());
}

TEST(PainterTest, PaintsBordersFromComputedStyle) {
    std::string_view html = "<html><body><div>Box</div></body></html>";
    Hummingbird::Core::ArenaAllocator arena(2048);
    Hummingbird::Html::Parser parser(arena, html);
    auto result = parser.parse();

    std::string css = "div { border-width: 2px; border-style: solid; border-color: red; }";
    Parser css_parser(css);
    auto sheet = css_parser.parse();
    StyleEngine engine;
    engine.apply(sheet, result.dom.get());

    Hummingbird::Layout::TreeBuilder builder;
    auto render_tree = builder.build(result.dom.get());
    ASSERT_NE(render_tree, nullptr);

    RecordingGraphicsContext context;
    Hummingbird::Layout::Rect viewport{0, 0, 200, 200};
    render_tree->layout(context, viewport);

    Hummingbird::Renderer::Painter painter;
    Hummingbird::Renderer::PaintOptions opts;
    painter.paint(*render_tree, context, opts);

    EXPECT_GE(context.fill_calls.size(), 4u);
}

TEST(PainterTest, PaintsOutsetBordersFromComputedStyle) {
    std::string_view html = "<html><body><div>Box</div></body></html>";
    Hummingbird::Core::ArenaAllocator arena(2048);
    Hummingbird::Html::Parser parser(arena, html);
    auto result = parser.parse();

    std::string css = "div { border-width: 2px; border-style: outset; border-color: red; }";
    Parser css_parser(css);
    auto sheet = css_parser.parse();
    StyleEngine engine;
    engine.apply(sheet, result.dom.get());

    Hummingbird::Layout::TreeBuilder builder;
    auto render_tree = builder.build(result.dom.get());
    ASSERT_NE(render_tree, nullptr);

    RecordingGraphicsContext context;
    Hummingbird::Layout::Rect viewport{0, 0, 200, 200};
    render_tree->layout(context, viewport);

    Hummingbird::Renderer::Painter painter;
    Hummingbird::Renderer::PaintOptions opts;
    painter.paint(*render_tree, context, opts);

    EXPECT_GE(context.fill_calls.size(), 4u);
}

TEST(PainterTest, PaintsRoundedBordersFromComputedStyle) {
    std::string_view html = "<html><body><div></div></body></html>";
    Hummingbird::Core::ArenaAllocator arena(2048);
    Hummingbird::Html::Parser parser(arena, html);
    auto result = parser.parse();

    std::string css =
        "div { width: 120px; height: 40px; background-color: #dde6ff; border-width: 2px; border-style: solid; "
        "border-color: #223366; border-radius: 10px; }";
    Parser css_parser(css);
    auto sheet = css_parser.parse();
    StyleEngine engine;
    engine.apply(sheet, result.dom.get());

    Hummingbird::Layout::TreeBuilder builder;
    auto render_tree = builder.build(result.dom.get());
    ASSERT_NE(render_tree, nullptr);

    RecordingGraphicsContext context;
    Hummingbird::Layout::Rect viewport{0, 0, 240, 180};
    render_tree->layout(context, viewport);

    Hummingbird::Renderer::Painter painter;
    Hummingbird::Renderer::PaintOptions opts;
    painter.paint(*render_tree, context, opts);

    ASSERT_FALSE(context.fill_calls.empty());
    float max_width = 0.0f;
    float min_width = 1000000.0f;
    for (const auto& call : context.fill_calls) {
        if (call.height <= 1.1f) {
            max_width = std::max(max_width, call.width);
            min_width = std::min(min_width, call.width);
        }
    }
    EXPECT_GT(max_width, 0.0f);
    EXPECT_GT(max_width - min_width, 1.0f);
}

TEST(PainterTest, PaintsOutlineOutsideBorderBox) {
    std::string_view html = "<html><body><div></div></body></html>";
    Hummingbird::Core::ArenaAllocator arena(2048);
    Hummingbird::Html::Parser parser(arena, html);
    auto result = parser.parse();

    std::string css =
        "div { width: 80px; height: 24px; border-width: 1px; border-style: solid; border-color: #000; outline: 3px "
        "solid #336699; outline-offset: 2px; }";
    Parser css_parser(css);
    auto sheet = css_parser.parse();
    StyleEngine engine;
    engine.apply(sheet, result.dom.get());

    Hummingbird::Layout::TreeBuilder builder;
    auto render_tree = builder.build(result.dom.get());
    ASSERT_NE(render_tree, nullptr);

    RecordingGraphicsContext context;
    Hummingbird::Layout::Rect viewport{0, 0, 240, 180};
    render_tree->layout(context, viewport);

    Hummingbird::Renderer::Painter painter;
    Hummingbird::Renderer::PaintOptions opts;
    painter.paint(*render_tree, context, opts);

    bool saw_outline_outside = false;
    for (const auto& call : context.fill_calls) {
        if (call.x < 0.0f || call.y < 0.0f) {
            saw_outline_outside = true;
            break;
        }
    }
    EXPECT_TRUE(saw_outline_outside);
}

namespace {
bool rect_matches(const Hummingbird::Layout::Rect& a, const Hummingbird::Layout::Rect& b) {
    constexpr float kEpsilon = 0.01f;
    return std::fabs(a.x - b.x) < kEpsilon && std::fabs(a.y - b.y) < kEpsilon &&
           std::fabs(a.width - b.width) < kEpsilon && std::fabs(a.height - b.height) < kEpsilon;
}

Hummingbird::Layout::Rect absolute_rect_for(const Hummingbird::Layout::RenderObject* node) {
    Hummingbird::Layout::Rect rect = node->get_rect();
    const auto* parent = node->get_parent();
    while (parent) {
        const auto& parent_rect = parent->get_rect();
        rect.x += parent_rect.x;
        rect.y += parent_rect.y;
        parent = parent->get_parent();
    }
    return rect;
}

Hummingbird::Layout::RenderObject* find_tag(Hummingbird::Layout::RenderObject* node, std::string_view tag) {
    if (!node) return nullptr;
    if (auto* element = dynamic_cast<const Hummingbird::DOM::Element*>(node->get_dom_node())) {
        if (element->get_tag_name() == tag) {
            return node;
        }
    }
    for (const auto& child : node->get_children()) {
        if (auto* found = find_tag(child.get(), tag)) {
            return found;
        }
    }
    return nullptr;
}

Hummingbird::Layout::TextBox* find_text_box(Hummingbird::Layout::RenderObject* node) {
    if (!node) return nullptr;
    if (auto* text = dynamic_cast<Hummingbird::Layout::TextBox*>(node)) {
        return text;
    }
    for (const auto& child : node->get_children()) {
        if (auto* found = find_text_box(child.get())) {
            return found;
        }
    }
    return nullptr;
}
}  // namespace

TEST(PainterTest, PaintsBackgroundImage) {
    std::string_view html = "<html><body><div>Box</div></body></html>";
    Hummingbird::Core::ArenaAllocator arena(2048);
    Hummingbird::Html::Parser parser(arena, html);
    auto result = parser.parse();

    std::string css =
        "div { width: 120px; height: 40px; background-image: url(/img/bg.png); background-repeat: no-repeat; }";
    Parser css_parser(css);
    auto sheet = css_parser.parse();
    StyleEngine engine;
    engine.apply(sheet, result.dom.get());

    Hummingbird::Layout::TreeBuilder builder;
    auto render_tree = builder.build(result.dom.get());
    ASSERT_NE(render_tree, nullptr);

    auto* div = find_tag(render_tree.get(), Hummingbird::Html::TagNames::Div);
    ASSERT_NE(div, nullptr);

    ImageBitmap bitmap;
    bitmap.width = 8;
    bitmap.height = 8;
    div->set_background_image(&bitmap);

    RecordingGraphicsContext context;
    Hummingbird::Layout::Rect viewport{0, 0, 200, 200};
    render_tree->layout(context, viewport);

    Hummingbird::Renderer::Painter painter;
    Hummingbird::Renderer::PaintOptions opts;
    painter.paint(*render_tree, context, opts);

    EXPECT_GE(context.image_calls, 1);
}

TEST(PainterTest, AppliesOpacityToSubtreePaint) {
    std::string_view html = "<html><body><div id='faded'>A</div><div>B</div></body></html>";
    Hummingbird::Core::ArenaAllocator arena(2048);
    Hummingbird::Html::Parser parser(arena, html);
    auto result = parser.parse();

    std::string css = "#faded { opacity: 0.25; }";
    Parser css_parser(css);
    auto sheet = css_parser.parse();
    StyleEngine engine;
    engine.apply(sheet, result.dom.get());

    Hummingbird::Layout::TreeBuilder builder;
    auto render_tree = builder.build(result.dom.get());
    ASSERT_NE(render_tree, nullptr);

    RecordingGraphicsContext context;
    Hummingbird::Layout::Rect viewport{0, 0, 300, 200};
    render_tree->layout(context, viewport);

    Hummingbird::Renderer::Painter painter;
    Hummingbird::Renderer::PaintOptions opts;
    painter.paint(*render_tree, context, opts);

    ASSERT_GE(context.draw_alphas.size(), 2u);
    bool saw_faded = false;
    bool saw_opaque = false;
    for (float alpha : context.draw_alphas) {
        saw_faded = saw_faded || std::fabs(alpha - 0.25f) < 0.01f;
        saw_opaque = saw_opaque || std::fabs(alpha - 1.0f) < 0.01f;
    }
    EXPECT_TRUE(saw_faded);
    EXPECT_TRUE(saw_opaque);
}

TEST(PainterTest, HonorsUnderlineThicknessAndOffset) {
    std::string_view html = "<html><body><p class='u'>Hi</p></body></html>";
    Hummingbird::Core::ArenaAllocator arena(2048);
    Hummingbird::Html::Parser parser(arena, html);
    auto result = parser.parse();

    std::string css =
        "p.u { text-decoration: underline; text-decoration-thickness: 3px; "
        "text-underline-offset: 4px; }";
    Parser css_parser(css);
    auto sheet = css_parser.parse();
    StyleEngine engine;
    engine.apply(sheet, result.dom.get());

    Hummingbird::Layout::TreeBuilder builder;
    auto render_tree = builder.build(result.dom.get());
    ASSERT_NE(render_tree, nullptr);

    auto* text_box = find_text_box(render_tree.get());
    ASSERT_NE(text_box, nullptr);

    RecordingGraphicsContext context;
    Hummingbird::Layout::Rect viewport{0, 0, 200, 200};
    render_tree->layout(context, viewport);

    Hummingbird::Renderer::Painter painter;
    Hummingbird::Renderer::PaintOptions opts;
    painter.paint(*render_tree, context, opts);

    ASSERT_FALSE(context.fill_calls.empty());
    const auto& underline_rect = context.fill_calls.back();

    auto text_rect = absolute_rect_for(text_box);
    float expected_y = text_rect.y + 12.0f + 4.0f;
    EXPECT_NEAR(underline_rect.y, expected_y, 0.01f);
    EXPECT_NEAR(underline_rect.height, 3.0f, 0.01f);
    EXPECT_GT(underline_rect.width, 0.0f);
}

TEST(PainterTest, AlignsUnderlinesAcrossInlineRuns) {
    std::string_view html = "<html><body><p class='u'>Big <span class='small'>small</span></p></body></html>";
    Hummingbird::Core::ArenaAllocator arena(2048);
    Hummingbird::Html::Parser parser(arena, html);
    auto result = parser.parse();

    std::string css = "p.u { text-decoration: underline; font-size: 20px; } .small { font-size: 10px; }";
    Parser css_parser(css);
    auto sheet = css_parser.parse();
    StyleEngine engine;
    engine.apply(sheet, result.dom.get());

    Hummingbird::Layout::TreeBuilder builder;
    auto render_tree = builder.build(result.dom.get());
    ASSERT_NE(render_tree, nullptr);

    FontAwareGraphicsContext context;
    Hummingbird::Layout::Rect viewport{0, 0, 300, 200};
    render_tree->layout(context, viewport);

    Hummingbird::Renderer::Painter painter;
    Hummingbird::Renderer::PaintOptions opts;
    painter.paint(*render_tree, context, opts);

    std::vector<float> underline_ys;
    for (const auto& rect : context.fill_calls) {
        if (std::abs(rect.height - 1.0f) < 0.01f && rect.width > 0.0f) {
            underline_ys.push_back(rect.y);
        }
    }

    ASSERT_GE(underline_ys.size(), 2u);
    float min_y = underline_ys.front();
    float max_y = underline_ys.front();
    for (float y : underline_ys) {
        min_y = std::min(min_y, y);
        max_y = std::max(max_y, y);
    }
    EXPECT_LE(max_y - min_y, 1.5f);
}

TEST(PainterTest, PaintsBorderEdgesAtComputedPositions) {
    std::string_view html = "<html><body><div>Box</div></body></html>";
    Hummingbird::Core::ArenaAllocator arena(2048);
    Hummingbird::Html::Parser parser(arena, html);
    auto result = parser.parse();

    std::string css = "div { border-width: 2px; border-style: solid; border-color: red; }";
    Parser css_parser(css);
    auto sheet = css_parser.parse();
    StyleEngine engine;
    engine.apply(sheet, result.dom.get());

    Hummingbird::Layout::TreeBuilder builder;
    auto render_tree = builder.build(result.dom.get());
    ASSERT_NE(render_tree, nullptr);

    RecordingGraphicsContext context;
    Hummingbird::Layout::Rect viewport{0, 0, 200, 200};
    render_tree->layout(context, viewport);

    auto* div_node = find_tag(render_tree.get(), Hummingbird::Html::TagNames::Div);
    ASSERT_NE(div_node, nullptr);
    const auto* style = div_node->get_computed_style();
    ASSERT_NE(style, nullptr);

    const auto absolute = absolute_rect_for(div_node);
    const auto& bw = style->border_width;

    Hummingbird::Renderer::Painter painter;
    Hummingbird::Renderer::PaintOptions opts;
    painter.paint(*render_tree, context, opts);

    ASSERT_GE(context.fill_calls.size(), 4u);
    Hummingbird::Layout::Rect expected_top{absolute.x, absolute.y, absolute.width, bw.top};
    Hummingbird::Layout::Rect expected_bottom{absolute.x, absolute.y + absolute.height - bw.bottom, absolute.width,
                                              bw.bottom};
    Hummingbird::Layout::Rect expected_left{absolute.x, absolute.y, bw.left, absolute.height};
    Hummingbird::Layout::Rect expected_right{absolute.x + absolute.width - bw.right, absolute.y, bw.right,
                                             absolute.height};

    bool found_top = false;
    bool found_bottom = false;
    bool found_left = false;
    bool found_right = false;

    for (const auto& rect : context.fill_calls) {
        found_top = found_top || rect_matches(rect, expected_top);
        found_bottom = found_bottom || rect_matches(rect, expected_bottom);
        found_left = found_left || rect_matches(rect, expected_left);
        found_right = found_right || rect_matches(rect, expected_right);
    }

    EXPECT_TRUE(found_top);
    EXPECT_TRUE(found_bottom);
    EXPECT_TRUE(found_left);
    EXPECT_TRUE(found_right);
}

TEST(PainterTest, PaintsBackgroundForBoxes) {
    std::string_view html = "<html><body><div>Box</div></body></html>";
    Hummingbird::Core::ArenaAllocator arena(2048);
    Hummingbird::Html::Parser parser(arena, html);
    auto result = parser.parse();

    std::string css = "div { background-color: #cccccc; }";
    Parser css_parser(css);
    auto sheet = css_parser.parse();
    StyleEngine engine;
    engine.apply(sheet, result.dom.get());

    Hummingbird::Layout::TreeBuilder builder;
    auto render_tree = builder.build(result.dom.get());
    ASSERT_NE(render_tree, nullptr);

    RecordingGraphicsContext context;
    Hummingbird::Layout::Rect viewport{0, 0, 200, 200};
    render_tree->layout(context, viewport);

    auto* div_node = find_tag(render_tree.get(), Hummingbird::Html::TagNames::Div);
    ASSERT_NE(div_node, nullptr);
    const auto expected = absolute_rect_for(div_node);

    Hummingbird::Renderer::Painter painter;
    Hummingbird::Renderer::PaintOptions opts;
    painter.paint(*render_tree, context, opts);

    bool found = false;
    for (const auto& rect : context.fill_calls) {
        if (rect_matches(rect, expected)) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST(PainterTest, PaintsBackgroundForInlineCode) {
    std::string_view html = "<html><body><p>Inline <code>code</code> text</p></body></html>";
    Hummingbird::Core::ArenaAllocator arena(2048);
    Hummingbird::Html::Parser parser(arena, html);
    auto result = parser.parse();

    std::string css = "code { background: #eeeeee; }";
    Parser css_parser(css);
    auto sheet = css_parser.parse();
    StyleEngine engine;
    engine.apply(sheet, result.dom.get());

    Hummingbird::Layout::TreeBuilder builder;
    auto render_tree = builder.build(result.dom.get());
    ASSERT_NE(render_tree, nullptr);

    RecordingGraphicsContext context;
    Hummingbird::Layout::Rect viewport{0, 0, 300, 200};
    render_tree->layout(context, viewport);

    auto* code_node = find_tag(render_tree.get(), Hummingbird::Html::TagNames::Code);
    ASSERT_NE(code_node, nullptr);
    const auto expected = absolute_rect_for(code_node);

    Hummingbird::Renderer::Painter painter;
    Hummingbird::Renderer::PaintOptions opts;
    painter.paint(*render_tree, context, opts);

    bool found = false;
    for (const auto& rect : context.fill_calls) {
        if (rect_matches(rect, expected)) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST(PainterTest, PaintsBackgroundForBlockCode) {
    std::string_view html = "<html><body><code>Block code</code></body></html>";
    Hummingbird::Core::ArenaAllocator arena(2048);
    Hummingbird::Html::Parser parser(arena, html);
    auto result = parser.parse();

    std::string css = "code { display: block; background: #eeeeee; }";
    Parser css_parser(css);
    auto sheet = css_parser.parse();
    StyleEngine engine;
    engine.apply(sheet, result.dom.get());

    Hummingbird::Layout::TreeBuilder builder;
    auto render_tree = builder.build(result.dom.get());
    ASSERT_NE(render_tree, nullptr);

    RecordingGraphicsContext context;
    Hummingbird::Layout::Rect viewport{0, 0, 300, 200};
    render_tree->layout(context, viewport);

    auto* code_node = find_tag(render_tree.get(), Hummingbird::Html::TagNames::Code);
    ASSERT_NE(code_node, nullptr);
    const auto expected = absolute_rect_for(code_node);

    Hummingbird::Renderer::Painter painter;
    Hummingbird::Renderer::PaintOptions opts;
    painter.paint(*render_tree, context, opts);

    bool found = false;
    for (const auto& rect : context.fill_calls) {
        if (rect_matches(rect, expected)) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST(PainterTest, PaintsImagePlaceholderWithAltText) {
    std::string_view html = "<html><body><img alt=\"Logo\" width=\"32\" height=\"16\"></body></html>";
    Hummingbird::Core::ArenaAllocator arena(2048);
    Hummingbird::Html::Parser parser(arena, html);
    auto result = parser.parse();

    Hummingbird::Layout::TreeBuilder builder;
    auto render_tree = builder.build(result.dom.get());
    ASSERT_NE(render_tree, nullptr);

    RecordingGraphicsContext context;
    Hummingbird::Layout::Rect viewport{0, 0, 200, 200};
    render_tree->layout(context, viewport);

    Hummingbird::Renderer::Painter painter;
    Hummingbird::Renderer::PaintOptions opts;
    painter.paint(*render_tree, context, opts);

    EXPECT_EQ(context.draw_calls, 1);
    EXPECT_EQ(context.last_text, "Logo");
    EXPECT_GE(context.fill_calls.size(), 4u);
}

TEST(PainterTest, SkipsPaintForOffscreenNodes) {
    std::string_view html = "<html><body><p>First</p><p>Second</p></body></html>";
    Hummingbird::Core::ArenaAllocator arena(2048);
    Hummingbird::Html::Parser parser(arena, html);
    auto result = parser.parse();

    Hummingbird::Layout::TreeBuilder builder;
    auto render_tree = builder.build(result.dom.get());
    ASSERT_NE(render_tree, nullptr);

    RecordingGraphicsContext context;
    Hummingbird::Layout::Rect viewport{0, 0, 200, 15};
    render_tree->layout(context, viewport);

    Hummingbird::Renderer::Painter painter;
    Hummingbird::Renderer::PaintOptions opts;
    opts.viewport = viewport;
    painter.paint(*render_tree, context, opts);

    EXPECT_EQ(context.draw_calls, 1);
    EXPECT_EQ(context.last_text, "First");
}

TEST(PainterTest, PaintsListMarkersWithCulling) {
    std::string_view html = "<html><body><ul><li>Item</li></ul></body></html>";
    Hummingbird::Core::ArenaAllocator arena(2048);
    Hummingbird::Html::Parser parser(arena, html);
    auto result = parser.parse();

    Hummingbird::Css::Stylesheet sheet;
    Hummingbird::Css::StyleEngine engine;
    engine.apply(sheet, result.dom.get());

    Hummingbird::Layout::TreeBuilder builder;
    auto render_tree = builder.build(result.dom.get());
    ASSERT_NE(render_tree, nullptr);

    RecordingGraphicsContext context;
    Hummingbird::Layout::Rect viewport{0, 0, 200, 200};
    render_tree->layout(context, viewport);

    Hummingbird::Layout::RenderListItem* list_item = nullptr;
    std::function<void(Hummingbird::Layout::RenderObject*)> find_list_item =
        [&](Hummingbird::Layout::RenderObject* node) {
            if (!node || list_item) return;
            if (auto* item = dynamic_cast<Hummingbird::Layout::RenderListItem*>(node)) {
                list_item = item;
                return;
            }
            for (const auto& child : node->get_children()) {
                find_list_item(child.get());
            }
        };
    find_list_item(render_tree.get());
    ASSERT_NE(list_item, nullptr);
    EXPECT_GT(list_item->marker_rect().width, 0.0f);

    Hummingbird::Renderer::Painter painter;
    Hummingbird::Renderer::PaintOptions opts;
    opts.viewport = viewport;
    painter.paint(*render_tree, context, opts);

    EXPECT_FALSE(context.fill_calls.empty());
}

TEST(PainterTest, PaintsOrderedListMarkerText) {
    std::string_view html = "<html><body><ol><li>Item</li></ol></body></html>";
    Hummingbird::Core::ArenaAllocator arena(2048);
    Hummingbird::Html::Parser parser(arena, html);
    auto result = parser.parse();

    Hummingbird::Css::Stylesheet sheet;
    Hummingbird::Css::StyleEngine engine;
    engine.apply(sheet, result.dom.get());

    Hummingbird::Layout::TreeBuilder builder;
    auto render_tree = builder.build(result.dom.get());
    ASSERT_NE(render_tree, nullptr);

    RecordingGraphicsContext context;
    Hummingbird::Layout::Rect viewport{0, 0, 200, 200};
    render_tree->layout(context, viewport);

    Hummingbird::Renderer::Painter painter;
    Hummingbird::Renderer::PaintOptions opts;
    opts.viewport = viewport;
    painter.paint(*render_tree, context, opts);

    bool saw_marker = false;
    for (const auto& text : context.drawn_texts) {
        if (text == "1.") {
            saw_marker = true;
            break;
        }
    }
    EXPECT_TRUE(saw_marker);
}

TEST(PainterTest, PaintsHorizontalRuleWithCulling) {
    std::string_view html = "<html><body><hr></body></html>";
    Hummingbird::Core::ArenaAllocator arena(2048);
    Hummingbird::Html::Parser parser(arena, html);
    auto result = parser.parse();

    Hummingbird::Layout::TreeBuilder builder;
    auto render_tree = builder.build(result.dom.get());
    ASSERT_NE(render_tree, nullptr);

    RecordingGraphicsContext context;
    Hummingbird::Layout::Rect viewport{0, 0, 200, 200};
    render_tree->layout(context, viewport);

    Hummingbird::Renderer::Painter painter;
    Hummingbird::Renderer::PaintOptions opts;
    opts.viewport = viewport;
    painter.paint(*render_tree, context, opts);

    EXPECT_FALSE(context.fill_calls.empty());
}

TEST(PainterTest, PaintsTableCellGridLinesWithoutAuthorCss) {
    std::string_view html = "<html><body><table border='1'><tr><td>A</td><td>B</td></tr></table></body></html>";
    Hummingbird::Core::ArenaAllocator arena(2048);
    Hummingbird::Html::Parser parser(arena, html);
    auto result = parser.parse();

    Hummingbird::Css::Stylesheet sheet;
    Hummingbird::Css::StyleEngine engine;
    engine.apply(sheet, result.dom.get());

    Hummingbird::Layout::TreeBuilder builder;
    auto render_tree = builder.build(result.dom.get());
    ASSERT_NE(render_tree, nullptr);

    RecordingGraphicsContext context;
    Hummingbird::Layout::Rect viewport{0, 0, 240, 180};
    render_tree->layout(context, viewport);

    Hummingbird::Renderer::Painter painter;
    Hummingbird::Renderer::PaintOptions opts;
    opts.viewport = viewport;
    painter.paint(*render_tree, context, opts);

    // Two cells * 4 grid edges each.
    EXPECT_GE(context.fill_calls.size(), 8u);
}

TEST(PainterTest, SkipsFallbackGridForLikelyLayoutTableWithoutBorderHints) {
    std::string_view html = "<html><body><table><tr><td>A</td><td>B</td></tr></table></body></html>";
    Hummingbird::Core::ArenaAllocator arena(2048);
    Hummingbird::Html::Parser parser(arena, html);
    auto result = parser.parse();

    Hummingbird::Css::Stylesheet sheet;
    Hummingbird::Css::StyleEngine engine;
    engine.apply(sheet, result.dom.get());

    Hummingbird::Layout::TreeBuilder builder;
    auto render_tree = builder.build(result.dom.get());
    ASSERT_NE(render_tree, nullptr);

    RecordingGraphicsContext context;
    Hummingbird::Layout::Rect viewport{0, 0, 240, 180};
    render_tree->layout(context, viewport);

    Hummingbird::Renderer::Painter painter;
    Hummingbird::Renderer::PaintOptions opts;
    opts.viewport = viewport;
    painter.paint(*render_tree, context, opts);

    EXPECT_EQ(context.fill_calls.size(), 0u);
}

// background-size geometry (compute_background_image_rect): the DDG logo bug
// where `background-size: 100%%` squished the SVG into an ellipse.
TEST(BackgroundSizeGeometryTest, PercentFillsWidthAndPreservesAspect) {
    ImageBitmap image;
    image.width = 200;
    image.height = 200;  // square (duck-in-circle)
    ComputedStyle style;
    style.background_size.type = ComputedStyle::BackgroundSize::Type::Length;
    style.background_size.width = 100.0f;
    style.background_size.width_is_percent = true;  // 100%, height auto

    const Hummingbird::Layout::Rect area{0, 0, 205, 200};
    const auto dest = Hummingbird::Layout::PaintUtils::compute_background_image_rect(area, image, style);
    EXPECT_FLOAT_EQ(dest.width, 205.0f);   // 100% of the box width
    EXPECT_FLOAT_EQ(dest.height, 205.0f);  // aspect preserved -> circle stays a circle
}

TEST(BackgroundSizeGeometryTest, PercentPortraitImageKeepsAspect) {
    ImageBitmap image;
    image.width = 200;
    image.height = 250;  // portrait (duck + wordmark)
    ComputedStyle style;
    style.background_size.type = ComputedStyle::BackgroundSize::Type::Length;
    style.background_size.width = 100.0f;
    style.background_size.width_is_percent = true;

    const Hummingbird::Layout::Rect area{0, 0, 205, 200};
    const auto dest = Hummingbird::Layout::PaintUtils::compute_background_image_rect(area, image, style);
    EXPECT_FLOAT_EQ(dest.width, 205.0f);
    EXPECT_NEAR(dest.height, 256.25f, 0.1f);  // 250 * (205/200), aspect preserved
}

TEST(BackgroundSizeGeometryTest, ExplicitTwoValuePercentResolvesBothAxes) {
    ImageBitmap image;
    image.width = 200;
    image.height = 200;
    ComputedStyle style;
    style.background_size.type = ComputedStyle::BackgroundSize::Type::Length;
    style.background_size.width = 50.0f;
    style.background_size.width_is_percent = true;
    style.background_size.height = 25.0f;
    style.background_size.height_is_percent = true;

    const Hummingbird::Layout::Rect area{0, 0, 200, 200};
    const auto dest = Hummingbird::Layout::PaintUtils::compute_background_image_rect(area, image, style);
    EXPECT_FLOAT_EQ(dest.width, 100.0f);  // 50% of 200
    EXPECT_FLOAT_EQ(dest.height, 50.0f);  // 25% of 200
}

namespace {
// Records clip push/pop and image draws to verify background-clip behavior.
class ClipRecordingContext : public IGraphicsContext {
public:
    void set_viewport(const Hummingbird::Layout::Rect&) override {}
    void clear(const Color&) override {}
    void present() override {}
    void fill_rect(const Hummingbird::Layout::Rect&, const Color&) override {}
    void draw_image(const ImageBitmap&, const Hummingbird::Layout::Rect& dest) override {
        image_dests.push_back(dest);
        clipped_when_drawn.push_back(clip_depth > 0);
    }
    TextMetrics measure_text(const std::string&, const TextStyle&) override { return {}; }
    void draw_text(const std::string&, float, float, const TextStyle&) override {}
    void push_clip(const Hummingbird::Layout::Rect& rect) override {
        pushed.push_back(rect);
        ++clip_depth;
    }
    void pop_clip() override { --clip_depth; }

    std::vector<Hummingbird::Layout::Rect> image_dests;
    std::vector<Hummingbird::Layout::Rect> pushed;
    std::vector<bool> clipped_when_drawn;
    int clip_depth = 0;
};
}  // namespace

TEST(BackgroundClipTest, BackgroundImageIsClippedToItsBox) {
    ClipRecordingContext ctx;
    ImageBitmap image;
    image.width = 100;
    image.height = 100;  // square image
    ComputedStyle style;
    style.background_image = "logo.svg";
    style.background_size.type = ComputedStyle::BackgroundSize::Type::Length;
    style.background_size.width = 100.0f;
    style.background_size.width_is_percent = true;  // 100% -> 200 wide, 200 tall
    style.background_repeat = ComputedStyle::BackgroundRepeat::NoRepeat;

    const Hummingbird::Layout::Rect area{0, 0, 200, 80};  // wide/short box
    Hummingbird::Layout::PaintUtils::draw_background_image(ctx, area, image, style);

    // A clip matching the box was pushed, the image drawn under it, clip balanced.
    ASSERT_EQ(ctx.pushed.size(), 1u);
    EXPECT_FLOAT_EQ(ctx.pushed[0].width, 200.0f);
    EXPECT_FLOAT_EQ(ctx.pushed[0].height, 80.0f);
    ASSERT_EQ(ctx.image_dests.size(), 1u);
    EXPECT_TRUE(ctx.clipped_when_drawn[0]);
    EXPECT_EQ(ctx.clip_depth, 0);  // push/pop balanced
    // The image is genuinely taller than the box, so the clip is what crops it.
    EXPECT_GT(ctx.image_dests[0].height, area.height);
}
