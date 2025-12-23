#include <gtest/gtest.h>
#include "renderer/Painter.h"
#include "layout/TreeBuilder.h"
#include "html/HtmlParser.h"
#include "core/IGraphicsContext.h"
#include "style/Parser.h"
#include "style/StyleEngine.h"

using Hummingbird::Layout::RenderObject;
using namespace Hummingbird::Css;

// Graphics context that records draw_text calls and provides deterministic metrics.
class RecordingGraphicsContext : public IGraphicsContext {
public:
    void set_viewport(const Hummingbird::Layout::Rect& viewport) override { viewport_ = viewport; }
    void clear(const Color&) override {}
    void present() override {}
    void fill_rect(const Hummingbird::Layout::Rect& rect, const Color&) override { fill_calls.push_back(rect); }

    TextMetrics measure_text(const std::string& text, const TextStyle&) override {
        return { static_cast<float>(text.size()) * 8.0f, 16.0f };
    }

    void draw_text(const std::string& text, float, float, const TextStyle&) override {
        ++draw_calls;
        last_text = text;
    }

    int draw_calls = 0;
    std::string last_text;
    std::vector<Hummingbird::Layout::Rect> fill_calls;
    Hummingbird::Layout::Rect viewport_{0, 0, 0, 0};
};

TEST(PainterIntegrationTest, PaintsTextNodesFromParserOutput) {
    // Arrange: parse a simple HTML snippet.
    std::string_view html = "<html><body><p>First line</p><p>Second line</p></body></html>";
    ArenaAllocator arena(2048);
    Hummingbird::Html::Parser parser(arena, html);
    auto dom = parser.parse();

    // Build render tree.
    Hummingbird::Layout::TreeBuilder builder;
    auto render_tree = builder.build(dom.get());
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
    ArenaAllocator arena(2048);
    Hummingbird::Html::Parser parser(arena, html);
    auto dom = parser.parse();

    Hummingbird::Layout::TreeBuilder builder;
    auto render_tree = builder.build(dom.get());
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
    ArenaAllocator arena(2048);
    Hummingbird::Html::Parser parser(arena, html);
    auto dom = parser.parse();

    std::string css = "div { border-width: 2px; border-style: solid; border-color: red; }";
    Parser css_parser(css);
    auto sheet = css_parser.parse();
    StyleEngine engine;
    engine.apply(sheet, dom.get());

    Hummingbird::Layout::TreeBuilder builder;
    auto render_tree = builder.build(dom.get());
    ASSERT_NE(render_tree, nullptr);

    RecordingGraphicsContext context;
    Hummingbird::Layout::Rect viewport{0, 0, 200, 200};
    render_tree->layout(context, viewport);

    Hummingbird::Renderer::Painter painter;
    Hummingbird::Renderer::PaintOptions opts;
    painter.paint(*render_tree, context, opts);

    EXPECT_GE(context.fill_calls.size(), 4u);
}
