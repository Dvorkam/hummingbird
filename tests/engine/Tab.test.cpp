#include "engine/Tab.h"

#include <gtest/gtest.h>

#include "core/platform_api/INetwork.h"
#include "core/platform_api/ResourceProviderFactory.h"
#include "layout/TestGraphicsContext.h"

namespace {

class InlineNetwork final : public INetwork {
public:
    explicit InlineNetwork(std::string body) : body_(std::move(body)) {}

    void get(const std::string& /*url*/, std::function<void(std::string)> callback) override {
        if (callback) callback(body_);
    }

    void shutdown() override {}

private:
    std::string body_;
};

}  // namespace

TEST(EngineTabTest, NavigateAndBuildsDocument) {
    const std::string html = R"HTML(
<!doctype html>
<html>
  <head>
    <meta charset="utf-8">
    <title>Tab Test</title>
    <style>
      body { margin: 10px; }
      p { color: #333; }
    </style>
  </head>
  <body>
    <p>Hello from Tab</p>
  </body>
</html>
)HTML";

    auto provider = create_resource_provider();
    ASSERT_NE(provider, nullptr);

    Hummingbird::Engine::Tab tab(std::make_unique<InlineNetwork>(html), std::make_unique<InlineNetwork>(html),
                                 std::move(provider));

    TestGraphicsContext context;
    Hummingbird::Layout::Rect viewport{0, 0, 800, 600};

    tab.navigate("https://example.dev");

    EXPECT_TRUE(tab.tick(context, viewport));
    tab.paint(context, viewport, false);

    EXPECT_FALSE(tab.tick(context, viewport));
    EXPECT_EQ(tab.requested_url(), "https://example.dev");
}
