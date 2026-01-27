#include "engine/DocumentPipeline.h"

#include <gtest/gtest.h>

#include <string>

#include "core/platform_api/ResourceProviderFactory.h"
#include "core/platform_api/ScriptEngineFactory.h"
#include "engine/ResourceStore.h"
#include "layout/Geometry.h"
#include "test_utils/TestGraphicsContext.h"

namespace {
using Hummingbird::Engine::DocumentPipeline;
using Hummingbird::Engine::ResourceStore;
using Hummingbird::Layout::Point;
using Hummingbird::Layout::Rect;
using Hummingbird::Test::TestGraphicsContext;
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
