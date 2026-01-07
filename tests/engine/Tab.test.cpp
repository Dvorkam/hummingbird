#include "engine/Tab.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/platform_api/IImageDecoder.h"
#include "core/platform_api/INetwork.h"
#include "core/platform_api/ResourceProviderFactory.h"
#include "layout/TestGraphicsContext.h"

namespace {

class InlineNetwork final : public INetwork {
public:
    InlineNetwork(std::string body, std::string effective_url = {})
        : body_(std::move(body)), effective_url_(std::move(effective_url)) {}

    void get(const std::string& url, std::function<void(NetworkResponse)> callback) override {
        NetworkResponse response;
        response.url = url;
        response.effective_url = effective_url_.empty() ? url : effective_url_;
        response.status = 200;
        response.body = body_;
        if (callback) callback(std::move(response));
    }

    void shutdown() override {}

private:
    std::string body_;
    std::string effective_url_;
};

class RoutingNetwork final : public INetwork {
public:
    void set_response(const std::string& url, std::string body) { responses_[url] = std::move(body); }

    void get(const std::string& url, std::function<void(NetworkResponse)> callback) override {
        requested_.push_back(url);
        auto it = responses_.find(url);
        NetworkResponse response;
        response.url = url;
        response.effective_url = url;
        if (it != responses_.end()) {
            response.status = 200;
            response.body = it->second;
        }
        if (callback) callback(std::move(response));
    }

    void shutdown() override {}

    const std::vector<std::string>& requested() const { return requested_; }

private:
    std::unordered_map<std::string, std::string> responses_;
    std::vector<std::string> requested_;
};

class DeferredNetwork final : public INetwork {
public:
    void set_response(const std::string& url, std::string body) { responses_[url] = std::move(body); }
    void defer_response(const std::string& url, std::string body) { deferred_[url] = std::move(body); }

    void get(const std::string& url, std::function<void(NetworkResponse)> callback) override {
        requested_.push_back(url);
        if (deferred_.find(url) != deferred_.end()) {
            pending_[url] = std::move(callback);
            return;
        }
        auto it = responses_.find(url);
        NetworkResponse response;
        response.url = url;
        response.effective_url = url;
        if (it != responses_.end()) {
            response.status = 200;
            response.body = it->second;
        }
        if (callback) callback(std::move(response));
    }

    void complete(const std::string& url) {
        auto pending_it = pending_.find(url);
        if (pending_it == pending_.end()) return;
        auto deferred_it = deferred_.find(url);
        std::string body = deferred_it == deferred_.end() ? std::string{} : deferred_it->second;
        auto callback = std::move(pending_it->second);
        pending_.erase(pending_it);
        NetworkResponse response;
        response.url = url;
        response.effective_url = url;
        if (!body.empty()) {
            response.status = 200;
            response.body = std::move(body);
        }
        if (callback) callback(std::move(response));
    }

    void shutdown() override {}

private:
    std::unordered_map<std::string, std::string> responses_;
    std::unordered_map<std::string, std::string> deferred_;
    std::unordered_map<std::string, std::function<void(NetworkResponse)>> pending_;
    std::vector<std::string> requested_;
};

class InlineImageDecoder final : public IImageDecoder {
public:
    std::optional<ImageBitmap> decode(std::string_view bytes) override {
        if (bytes.empty()) {
            return std::nullopt;
        }
        ImageBitmap bitmap;
        bitmap.width = 2;
        bitmap.height = 2;
        bitmap.stride = 8;
        bitmap.format = PixelFormat::PRGB32;
        bitmap.pixels.assign(static_cast<size_t>(bitmap.stride) * bitmap.height, 0xFF);
        return bitmap;
    }
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
                                 std::move(provider), nullptr);

    TestGraphicsContext context;
    Hummingbird::Layout::Rect viewport{0, 0, 800, 600};

    tab.navigate("https://example.dev");

    EXPECT_TRUE(tab.tick(context, viewport));
    tab.paint(context, viewport, false);

    EXPECT_FALSE(tab.tick(context, viewport));
    EXPECT_EQ(tab.requested_url(), "https://example.dev");
}

TEST(EngineTabTest, FetchesLinkedStylesheet) {
    const std::string html = R"HTML(
<!doctype html>
<html>
  <head>
    <link rel="stylesheet" href="styles/main.css">
  </head>
  <body>
    <p>Styled text</p>
  </body>
</html>
)HTML";

    auto provider = create_resource_provider();
    ASSERT_NE(provider, nullptr);

    auto network = std::make_unique<RoutingNetwork>();
    auto* network_ptr = network.get();
    network->set_response("https://acme.test", html);
    network->set_response("https://acme.test/styles/main.css", "p { color: #cc0000; }");

    auto fallback = std::make_unique<RoutingNetwork>();

    Hummingbird::Engine::Tab tab(std::move(network), std::move(fallback), std::move(provider), nullptr);

    TestGraphicsContext context;
    Hummingbird::Layout::Rect viewport{0, 0, 800, 600};

    tab.navigate("https://acme.test");
    tab.tick(context, viewport);
    tab.tick(context, viewport);

    auto view = tab.resource_view("https://acme.test/styles/main.css", Hummingbird::Engine::ResourceType::Stylesheet);
    ASSERT_TRUE(view.has_value());
    EXPECT_EQ(view->state, Hummingbird::Engine::ResourceState::Ready);
    EXPECT_EQ(view->body, "p { color: #cc0000; }");

    EXPECT_NE(std::find(network_ptr->requested().begin(), network_ptr->requested().end(),
                        "https://acme.test/styles/main.css"),
              network_ptr->requested().end());
}

TEST(EngineTabTest, FetchesLinkedImage) {
    const std::string html = R"HTML(
<!doctype html>
<html>
  <body>
    <img SRC="images/logo.png" alt="logo">
  </body>
</html>
)HTML";

    auto provider = create_resource_provider();
    ASSERT_NE(provider, nullptr);

    auto network = std::make_unique<RoutingNetwork>();
    auto* network_ptr = network.get();
    network->set_response("https://acme.test", html);
    network->set_response("https://acme.test/images/logo.png", "PNGDATA");

    auto fallback = std::make_unique<RoutingNetwork>();

    Hummingbird::Engine::Tab tab(std::move(network), std::move(fallback), std::move(provider),
                                 std::make_unique<InlineImageDecoder>());

    TestGraphicsContext context;
    Hummingbird::Layout::Rect viewport{0, 0, 800, 600};

    tab.navigate("https://acme.test");
    tab.tick(context, viewport);
    tab.tick(context, viewport);

    auto view = tab.resource_view("https://acme.test/images/logo.png", Hummingbird::Engine::ResourceType::Image);
    ASSERT_TRUE(view.has_value());
    EXPECT_EQ(view->state, Hummingbird::Engine::ResourceState::Ready);
    EXPECT_EQ(view->body, "PNGDATA");
    ASSERT_NE(view->image, nullptr);
    EXPECT_EQ(view->image->width, 2);
    EXPECT_EQ(view->image->height, 2);

    EXPECT_NE(std::find(network_ptr->requested().begin(), network_ptr->requested().end(),
                        "https://acme.test/images/logo.png"),
              network_ptr->requested().end());
}

TEST(EngineTabTest, RebuildsWhenStylesheetArrives) {
    const std::string html = R"HTML(
<!doctype html>
<html>
  <head>
    <link rel="stylesheet" href="styles/main.css">
  </head>
  <body>
    <p>Styled text</p>
  </body>
</html>
)HTML";

    auto provider = create_resource_provider();
    ASSERT_NE(provider, nullptr);

    auto network = std::make_unique<DeferredNetwork>();
    auto* network_ptr = network.get();
    network->set_response("https://acme.test", html);
    network->defer_response("https://acme.test/styles/main.css", "p { color: blue; }");

    auto fallback = std::make_unique<DeferredNetwork>();

    Hummingbird::Engine::Tab tab(std::move(network), std::move(fallback), std::move(provider), nullptr);

    TestGraphicsContext context;
    Hummingbird::Layout::Rect viewport{0, 0, 800, 600};

    tab.navigate("https://acme.test");
    EXPECT_TRUE(tab.tick(context, viewport));
    EXPECT_FALSE(tab.tick(context, viewport));

    auto view = tab.resource_view("https://acme.test/styles/main.css", Hummingbird::Engine::ResourceType::Stylesheet);
    ASSERT_TRUE(view.has_value());
    EXPECT_EQ(view->state, Hummingbird::Engine::ResourceState::Loading);

    network_ptr->complete("https://acme.test/styles/main.css");
    EXPECT_TRUE(tab.tick(context, viewport));
}

TEST(EngineTabTest, HitTestResolvesLink) {
    const std::string html = R"HTML(
<!doctype html>
<html>
  <body>
    <a HREF="/next">Next</a>
  </body>
</html>
)HTML";

    auto provider = create_resource_provider();
    ASSERT_NE(provider, nullptr);

    Hummingbird::Engine::Tab tab(std::make_unique<InlineNetwork>(html), std::make_unique<InlineNetwork>(html),
                                 std::move(provider), nullptr);

    TestGraphicsContext context;
    Hummingbird::Layout::Rect viewport{0, 0, 200, 200};

    tab.navigate("https://example.dev");
    EXPECT_TRUE(tab.tick(context, viewport));

    Hummingbird::Layout::Point point{10.0f, 12.0f};
    auto link = tab.hit_test_link(point, viewport);
    ASSERT_TRUE(link.has_value());
    EXPECT_EQ(*link, "https://example.dev/next");
}

TEST(EngineTabTest, UpdatesRequestedUrlFromEffectiveUrl) {
    const std::string html = R"HTML(
<!doctype html>
<html>
  <body>
    <p>Redirected</p>
  </body>
</html>
)HTML";

    auto provider = create_resource_provider();
    ASSERT_NE(provider, nullptr);

    const std::string effective_url = "https://www.acme.com/software/thttpd/";
    Hummingbird::Engine::Tab tab(std::make_unique<InlineNetwork>(html, effective_url),
                                 std::make_unique<InlineNetwork>(html, effective_url), std::move(provider), nullptr);

    TestGraphicsContext context;
    Hummingbird::Layout::Rect viewport{0, 0, 200, 200};

    tab.navigate("http://acme.com/software/thttpd");
    EXPECT_TRUE(tab.tick(context, viewport));

    EXPECT_EQ(tab.requested_url(), effective_url);
}
