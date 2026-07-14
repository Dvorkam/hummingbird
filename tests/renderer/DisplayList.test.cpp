#include "renderer/DisplayList.h"

#include <gtest/gtest.h>

#include <string_view>

#include "core/ArenaAllocator.h"
#include "core/platform_api/IGraphicsContext.h"
#include "html/HtmlParser.h"
#include "layout/TreeBuilder.h"
#include "renderer/Painter.h"

namespace {
using Hummingbird::Color;
using Hummingbird::IGraphicsContext;
using Hummingbird::ImageBitmap;
using Hummingbird::TextMetrics;
using Hummingbird::TextStyle;

class CountingGraphicsContext : public IGraphicsContext {
public:
    void set_viewport(const Hummingbird::Layout::Rect& /*viewport*/) override {}
    void clear(const Color& /*color*/) override {}
    void present() override {}
    void fill_rect(const Hummingbird::Layout::Rect& /*rect*/, const Color& /*color*/) override { ++fill_calls; }
    void draw_image(const ImageBitmap& /*image*/, const Hummingbird::Layout::Rect& /*dest*/) override { ++image_calls; }

    TextMetrics measure_text(const std::string& text, const TextStyle& /*style*/) override {
        return {static_cast<float>(text.size()) * 8.0f, 16.0f};
    }

    void draw_text(const std::string& /*text*/, float, float, const TextStyle&) override { ++draw_text_calls; }

    void draw_text_with_metrics(const std::string& /*text*/, float, float, const TextStyle&,
                                const TextMetrics& /*metrics*/) override {
        ++draw_text_with_metrics_calls;
    }
    void push_clip(const Hummingbird::Layout::Rect& rect) override {
        ++push_clip_calls;
        last_clip = rect;
    }
    void pop_clip() override { ++pop_clip_calls; }

    int fill_calls = 0;
    int image_calls = 0;
    int draw_text_calls = 0;
    int draw_text_with_metrics_calls = 0;
    int push_clip_calls = 0;
    int pop_clip_calls = 0;
    Hummingbird::Layout::Rect last_clip{};
};
}  // namespace

TEST(DisplayListTest, RecordsAndReplaysPaintCommands) {
    std::string_view html = "<html><body><p>Hello world</p></body></html>";
    Hummingbird::Core::ArenaAllocator arena(2048);
    Hummingbird::Html::Parser parser(arena, html);
    auto result = parser.parse();

    Hummingbird::Layout::TreeBuilder builder;
    auto render_tree = builder.build(result.dom.get());
    ASSERT_NE(render_tree, nullptr);

    CountingGraphicsContext baseline;
    Hummingbird::Layout::Rect viewport{0, 0, 800, 600};
    render_tree->layout(baseline, viewport);

    Hummingbird::Renderer::Painter painter;
    Hummingbird::Renderer::PaintOptions opts;
    opts.viewport = viewport;
    painter.paint(*render_tree, baseline, opts);

    Hummingbird::Renderer::DisplayList display_list;
    CountingGraphicsContext metrics_source;
    Hummingbird::Renderer::DisplayListRecorder recorder(display_list, metrics_source);
    painter.paint(*render_tree, recorder, opts);

    CountingGraphicsContext replay;
    display_list.replay(replay);

    EXPECT_EQ(replay.fill_calls, baseline.fill_calls);
    EXPECT_EQ(replay.image_calls, baseline.image_calls);
    EXPECT_EQ(replay.draw_text_calls, baseline.draw_text_calls);
    EXPECT_EQ(replay.draw_text_with_metrics_calls, baseline.draw_text_with_metrics_calls);
    EXPECT_GT(display_list.size(), 0u);
}

// Clip commands must survive the record/replay round-trip -- background-clip and
// overflow depend on it, and the recorder previously dropped them.
TEST(DisplayListTest, RecordsAndReplaysClipCommands) {
    Hummingbird::Renderer::DisplayList display_list;
    CountingGraphicsContext metrics_source;
    Hummingbird::Renderer::DisplayListRecorder recorder(display_list, metrics_source);

    recorder.push_clip(Hummingbird::Layout::Rect{10, 20, 100, 50});
    recorder.fill_rect(Hummingbird::Layout::Rect{0, 0, 200, 200}, Color{});
    recorder.pop_clip();

    CountingGraphicsContext replay;
    display_list.replay(replay);

    EXPECT_EQ(replay.push_clip_calls, 1);
    EXPECT_EQ(replay.pop_clip_calls, 1);
    EXPECT_EQ(replay.fill_calls, 1);
    EXPECT_FLOAT_EQ(replay.last_clip.width, 100.0f);
    EXPECT_FLOAT_EQ(replay.last_clip.height, 50.0f);
}
