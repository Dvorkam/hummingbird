#include <gtest/gtest.h>
#include "renderer/Painter.h"
#include "layout/TreeBuilder.h"
#include "html/HtmlParser.h"
#include "core/IGraphicsContext.h"

using Hummingbird::Layout::RenderObject;

// Graphics context that records draw_text calls and provides deterministic metrics.
class RecordingGraphicsContext : public IGraphicsContext {
public:
    void clear(const Color&) override {}
    void present() override {}
    void fill_rect(const Hummingbird::Layout::Rect&, const Color&) override {}

    TextMetrics measure_text(const std::string& text, const std::string&, float, bool, bool, bool) override {
        return { static_cast<float>(text.size()) * 8.0f, 16.0f };
    }

    void draw_text(const std::string& text, float, float, const std::string&, float, const Color&, bool, bool, bool) override {
        ++draw_calls;
        last_text = text;
    }

    int draw_calls = 0;
    std::string last_text;
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
    painter.paint(*render_tree, context);

    // Expect one draw call per non-whitespace text node.
    EXPECT_EQ(context.draw_calls, 2);
    EXPECT_EQ(context.last_text, "Second line");
}
