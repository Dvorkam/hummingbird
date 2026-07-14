#include "engine/document/DocumentPipeline.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "core/platform_api/IGraphicsContext.h"
#include "core/platform_api/ResourceProviderFactory.h"
#include "core/platform_api/ScriptEngineFactory.h"
#include "engine/resources/ResourceStore.h"
#include "layout/geometry/Geometry.h"
#include "test_utils/TestGraphicsContext.h"

namespace {
using Hummingbird::ImageBitmap;
using Hummingbird::PixelFormat;
using Hummingbird::Engine::DocumentPipeline;
using Hummingbird::Engine::ResourceStore;
using Hummingbird::Engine::ResourceType;
using Hummingbird::Layout::Point;
using Hummingbird::Layout::Rect;
using Hummingbird::Test::TestGraphicsContext;

class RecordingGraphicsContext : public Hummingbird::IGraphicsContext {
public:
    void set_viewport(const Hummingbird::Layout::Rect& /*viewport*/) override {}
    void clear(const Hummingbird::Color& /*color*/) override {}
    void present() override {}
    void fill_rect(const Hummingbird::Layout::Rect& /*rect*/, const Hummingbird::Color& /*color*/) override {}
    void draw_image(const ImageBitmap& /*image*/, const Hummingbird::Layout::Rect& /*dest*/) override { ++image_calls; }

    Hummingbird::TextMetrics measure_text(const std::string& text, const Hummingbird::TextStyle& style) override {
        const float font_size = style.font_size > 0.0f ? style.font_size : 16.0f;
        const float average_char_width = font_size * 0.5f;
        Hummingbird::TextMetrics metrics;
        metrics.width = static_cast<float>(text.size()) * average_char_width;
        metrics.height = font_size;
        metrics.ascent = font_size * 0.8f;
        metrics.descent = font_size * 0.2f;
        metrics.underline_position = -metrics.descent * 0.5f;
        metrics.underline_thickness = 1.0f;
        return metrics;
    }

    void draw_text(const std::string& text, float /*x*/, float /*y*/,
                   const Hummingbird::TextStyle& /*style*/) override {
        drawn_texts.push_back(text);
    }

    int image_calls = 0;
    std::vector<std::string> drawn_texts;
};
}  // namespace

TEST(DocumentPipelineTest, DispatchesLoadHandler) {
    const std::string html = R"HTML(
<!doctype html>
<html>
  <head>
    <style>
      body { margin: 0; padding: 0; }
    </style>
  </head>
  <body onload="const target = document.getElementById('status'); if (target) { target.textContent = 'loaded'; }">
    <p id="status">idle</p>
  </body>
</html>
)HTML";

    ResourceStore store;
    auto provider = Hummingbird::create_resource_provider();
    ASSERT_NE(provider, nullptr);
    auto engine = Hummingbird::create_script_engine();
    ASSERT_NE(engine, nullptr);

    DocumentPipeline pipeline(&store, provider.get(), nullptr, std::move(engine));
    TestGraphicsContext graphics;
    Rect viewport{0, 0, 200, 200};

    ASSERT_TRUE(pipeline.parse_html(html));
    pipeline.apply_styles_and_layout(graphics, viewport, "https://example.dev");

    auto result = pipeline.dispatch_load();
    EXPECT_TRUE(result.handled);
    EXPECT_TRUE(result.mutated);
}

TEST(DocumentPipelineTest, DetectsMediaBreakpointCrossOnResize) {
    // DDG gates its desktop layout behind (min-width: 864px)-style rules;
    // resizing across such a bound must trigger a restyle (T-MEDIA-RESIZE-1).
    const std::string html = R"HTML(
<!doctype html>
<html>
  <head>
    <style>
      body { margin: 0; }
      @media (min-width: 864px) {
        body { margin: 8px; }
      }
    </style>
  </head>
  <body><p>hi</p></body>
</html>
)HTML";

    ResourceStore store;
    auto provider = Hummingbird::create_resource_provider();
    ASSERT_NE(provider, nullptr);
    auto engine = Hummingbird::create_script_engine();
    ASSERT_NE(engine, nullptr);

    DocumentPipeline pipeline(&store, provider.get(), nullptr, std::move(engine));
    TestGraphicsContext graphics;
    Rect narrow{0, 0, 800, 600};

    ASSERT_TRUE(pipeline.parse_html(html));
    pipeline.apply_styles_and_layout(graphics, narrow, "https://example.dev");

    // Same side of the breakpoint: relayout is enough.
    EXPECT_FALSE(pipeline.needs_restyle_for_viewport({0, 0, 820, 600}));
    // Crossing 864px flips the rule: restyle required.
    EXPECT_TRUE(pipeline.needs_restyle_for_viewport({0, 0, 1024, 768}));

    // After restyling at the wide viewport, staying wide needs no restyle
    // but shrinking back across the bound does.
    pipeline.apply_styles_and_layout(graphics, {0, 0, 1024, 768}, "https://example.dev");
    EXPECT_FALSE(pipeline.needs_restyle_for_viewport({0, 0, 900, 700}));
    EXPECT_TRUE(pipeline.needs_restyle_for_viewport(narrow));
}

TEST(DocumentPipelineTest, DispatchesClickHandler) {
    const std::string html = R"HTML(
<!doctype html>
<html>
  <head>
    <style>
      body { margin: 0; padding: 0; }
      button { display: block; }
    </style>
  </head>
  <body>
    <button onclick="const target = document.getElementById('status'); if (target) { target.textContent = 'clicked'; }">
      Click
    </button>
    <p id="status">idle</p>
  </body>
</html>
)HTML";

    ResourceStore store;
    auto provider = Hummingbird::create_resource_provider();
    ASSERT_NE(provider, nullptr);
    auto engine = Hummingbird::create_script_engine();
    ASSERT_NE(engine, nullptr);

    DocumentPipeline pipeline(&store, provider.get(), nullptr, std::move(engine));
    TestGraphicsContext graphics;
    Rect viewport{0, 0, 200, 200};

    ASSERT_TRUE(pipeline.parse_html(html));
    pipeline.apply_styles_and_layout(graphics, viewport, "https://example.dev");

    DocumentPipeline::HitTestContext context{Point{5.0f, 5.0f}, viewport, "https://example.dev", 0.0f};
    auto result = pipeline.dispatch_click(context);
    EXPECT_TRUE(result.handled);
    EXPECT_TRUE(result.mutated);
}

TEST(DocumentPipelineTest, CollectsBackgroundImageLinksFromStyles) {
    const std::string html = R"HTML(
<!doctype html>
<html>
  <head>
    <style>
      div { background-image: url(/img/background.png); }
    </style>
  </head>
  <body>
    <div>Box</div>
  </body>
</html>
)HTML";

    ResourceStore store;
    auto provider = Hummingbird::create_resource_provider();
    ASSERT_NE(provider, nullptr);
    auto engine = Hummingbird::create_script_engine();
    ASSERT_NE(engine, nullptr);

    DocumentPipeline pipeline(&store, provider.get(), nullptr, std::move(engine));
    TestGraphicsContext graphics;
    Rect viewport{0, 0, 200, 200};

    ASSERT_TRUE(pipeline.parse_html(html));
    pipeline.apply_styles_and_layout(graphics, viewport, "https://example.dev");

    const auto& links = pipeline.background_image_links();
    ASSERT_EQ(links.size(), 1u);
    EXPECT_EQ(links[0], "/img/background.png");
}

TEST(DocumentPipelineTest, PaintsBackgroundImagesFromResources) {
    const std::string html = R"HTML(
<!doctype html>
<html>
  <head>
    <style>
      div { width: 10px; height: 10px; background-image: url(/img/background.png); }
    </style>
  </head>
  <body>
    <div>Box</div>
  </body>
</html>
)HTML";

    ResourceStore store;
    auto provider = Hummingbird::create_resource_provider();
    ASSERT_NE(provider, nullptr);
    auto engine = Hummingbird::create_script_engine();
    ASSERT_NE(engine, nullptr);

    ImageBitmap bitmap;
    bitmap.width = 1;
    bitmap.height = 1;
    bitmap.stride = 4;
    bitmap.format = PixelFormat::BGRA32;
    bitmap.pixels = {0, 0, 0, 255};
    const std::string image_url = "https://example.dev/img/background.png";
    ASSERT_TRUE(store.begin_request(image_url, ResourceType::Image));
    ASSERT_TRUE(store.mark_ready(image_url, ResourceType::Image, {}));
    ASSERT_TRUE(store.set_image(image_url, ResourceType::Image, std::move(bitmap)));

    DocumentPipeline pipeline(&store, provider.get(), nullptr, std::move(engine));
    RecordingGraphicsContext graphics;
    Rect viewport{0, 0, 200, 200};

    ASSERT_TRUE(pipeline.parse_html(html));
    pipeline.apply_styles_and_layout(graphics, viewport, "https://example.dev");

    DocumentPipeline::PaintContext context{viewport, false, 0.0f};
    pipeline.paint(graphics, context);

    EXPECT_GT(graphics.image_calls, 0);
}

TEST(DocumentPipelineTest, PaintsFocusedInputTextWhenContentBoxCollapses) {
    const std::string html = R"HTML(
<!doctype html>
<html>
  <head>
    <style>
      input { width: 280px; height: 10px; padding: 5px 7px; border: 1px solid #666; }
    </style>
  </head>
  <body>
    <input id="q" type="text" />
  </body>
</html>
)HTML";

    ResourceStore store;
    auto provider = Hummingbird::create_resource_provider();
    ASSERT_NE(provider, nullptr);
    auto engine = Hummingbird::create_script_engine();
    ASSERT_NE(engine, nullptr);

    DocumentPipeline pipeline(&store, provider.get(), nullptr, std::move(engine));
    RecordingGraphicsContext graphics;
    Rect viewport{0, 0, 400, 200};

    ASSERT_TRUE(pipeline.parse_html(html));
    pipeline.apply_styles_and_layout(graphics, viewport, "https://example.dev");

    DocumentPipeline::HitTestContext hit{Point{12.0f, 12.0f}, viewport, "https://example.dev", 0.0f};
    ASSERT_TRUE(pipeline.focus_input_at(hit));

    auto edit = pipeline.handle_text_input("hello");
    ASSERT_TRUE(edit.handled);

    DocumentPipeline::PaintContext paint{viewport, false, 0.0f};
    pipeline.paint(graphics, paint);

    bool saw_typed_text = false;
    for (const auto& text : graphics.drawn_texts) {
        if (text == "hello") {
            saw_typed_text = true;
            break;
        }
    }
    EXPECT_TRUE(saw_typed_text);
}

TEST(DocumentPipelineTest, ContentHeightIncludesDescendantsBeyondRootHeightClamp) {
    std::string rows;
    rows.reserve(2048);
    for (int i = 0; i < 80; ++i) {
        rows += "<div>row</div>";
    }

    const std::string html =
        "<!doctype html><html><head><style>"
        "body{margin:0;padding:0;max-height:50%;}"
        "div{margin:0;padding:0;}"
        "</style></head><body>" +
        rows + "</body></html>";

    ResourceStore store;
    auto provider = Hummingbird::create_resource_provider();
    ASSERT_NE(provider, nullptr);
    auto engine = Hummingbird::create_script_engine();
    ASSERT_NE(engine, nullptr);

    DocumentPipeline pipeline(&store, provider.get(), nullptr, std::move(engine));
    TestGraphicsContext graphics;
    Rect viewport{0, 0, 800, 600};

    ASSERT_TRUE(pipeline.parse_html(html));
    pipeline.apply_styles_and_layout(graphics, viewport, "https://example.dev");

    EXPECT_GT(pipeline.content_height(), 300.0f);
}
