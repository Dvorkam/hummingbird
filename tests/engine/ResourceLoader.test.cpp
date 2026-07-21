#include "engine/resources/ResourceLoader.h"

#include <gtest/gtest.h>

#include <chrono>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>

#include "engine/resources/ResourceRequestPlanner.h"
#include "engine/resources/ResourceUrl.h"
#include "platform/decoders/CompositeImageDecoder.h"
#include "platform/net/StubNetwork.h"
#include "test_utils/TestFakes.h"

namespace {
using Hummingbird::NetworkError;
using Hummingbird::NetworkRequestOptions;
using Hummingbird::NetworkResponse;
using Hummingbird::Engine::resolve_resource_url;
using Hummingbird::Engine::ResourceLoader;
using Hummingbird::Engine::ResourceType;
using Hummingbird::Platform::CompositeImageDecoder;

class FakeResourceProvider final : public Hummingbird::IResourceProvider {
public:
    std::optional<std::string> load_text(std::string_view resource_id) override {
        queried.emplace_back(resource_id);
        auto it = text_.find(std::string(resource_id));
        if (it == text_.end()) return std::nullopt;
        return it->second;
    }

    std::optional<std::string> load_bytes(std::string_view resource_id) override {
        queried.emplace_back(resource_id);
        auto it = bytes_.find(std::string(resource_id));
        if (it == bytes_.end()) return std::nullopt;
        return it->second;
    }

    std::unordered_map<std::string, std::string> text_;
    std::unordered_map<std::string, std::string> bytes_;
    std::vector<std::string> queried;
};

class CapturingNetwork final : public Hummingbird::INetwork {
public:
    void get(const std::string& url, std::function<void(NetworkResponse)> callback,
             const NetworkRequestOptions& options = {}) override {
        requests.emplace_back(Request{url, "GET", {}, options});
        NetworkResponse response;
        response.url = url;
        response.effective_url = url;
        response.body = body;
        response.error = error;
        response.headers = response_headers;
        if (callback) callback(std::move(response));
    }

    void post(const std::string& url, std::string_view body_data, std::function<void(NetworkResponse)> callback,
              const NetworkRequestOptions& options = {}) override {
        requests.emplace_back(Request{url, "POST", std::string(body_data), options});
        NetworkResponse response;
        response.url = url;
        response.effective_url = url;
        response.body = body;
        response.error = error;
        response.headers = response_headers;
        if (callback) callback(std::move(response));
    }

    void shutdown() override {}

    struct Request {
        std::string url;
        std::string method;
        std::string body;
        NetworkRequestOptions options;
    };

    std::vector<Request> requests;
    std::string body;
    Hummingbird::Core::HttpHeaders response_headers;
    NetworkError error = NetworkError::None;
};

class AnimatedImageDecoder final : public Hummingbird::IImageDecoder {
public:
    std::optional<Hummingbird::AnimatedImage> decode_animation(std::string_view bytes) override {
        if (bytes.empty()) {
            return std::nullopt;
        }
        Hummingbird::AnimatedImage animation;
        Hummingbird::ImageBitmap first{};
        first.width = 1;
        first.height = 1;
        first.stride = 4;
        first.format = Hummingbird::PixelFormat::BGRA32;
        first.pixels = {0, 0, 0, 255};
        Hummingbird::ImageBitmap second = first;
        second.pixels = {1, 0, 0, 255};
        animation.frames.push_back(first);
        animation.frames.push_back(second);
        animation.delays_ms = {100, 100};
        return animation;
    }

    std::optional<Hummingbird::ImageBitmap> decode(std::string_view) override { return std::nullopt; }
};
}  // namespace

TEST(ResourceTypeTableTest, EveryTypeHasAConsistentDescriptor) {
    // The descriptor table is what makes adding a resource type a one-entry
    // change (T-RESOURCE-TYPE-TABLE-1): the loader's request path and the
    // update processor's ready path both dispatch off it, so every enum value
    // must have a well-formed entry.
    using Hummingbird::Engine::kResourceTypeCount;
    using Hummingbird::Engine::ResourceRequestPlanning::request_options_for;

    for (size_t i = 0; i < kResourceTypeCount; ++i) {
        const auto type = static_cast<ResourceType>(i);
        const auto& options = request_options_for(type);
        EXPECT_EQ(options.type, type) << "table entry " << i << " maps to the wrong type";
        EXPECT_FALSE(options.type_label.empty()) << "table entry " << i << " needs a label";
        EXPECT_FALSE(options.attr_label.empty()) << "table entry " << i << " needs an attr label";
    }
}

TEST(ResourceLoaderTest, StylesheetAssetsMarkReadyWithoutNetwork) {
    auto provider = std::make_unique<FakeResourceProvider>();
    provider->text_["styles/site.css"] = "body { color: #111; }";

    auto network = std::make_unique<CapturingNetwork>();
    auto* network_ptr = network.get();

    ResourceLoader loader(std::move(network), std::make_unique<CapturingNetwork>(), std::move(provider), nullptr);

    const std::string base_url = "https://example.dev/page.html";
    loader.request_stylesheets({"styles/site.css"}, base_url);

    auto resolved = resolve_resource_url(base_url, "styles/site.css");
    auto view = loader.view(resolved.key, ResourceType::Stylesheet);
    ASSERT_TRUE(view.has_value());
    EXPECT_EQ(view->state, Hummingbird::Engine::ResourceState::Ready);
    EXPECT_EQ(view->body, "body { color: #111; }");
    EXPECT_TRUE(network_ptr->requests.empty());

    auto batch = loader.consume_pending_updates();
    EXPECT_EQ(batch.pending_count, 0u);
}

TEST(ResourceLoaderTest, AboutBookmarksServesBuiltInPageWithoutNetwork) {
    // 7.6.2: about:bookmarks is a built-in page rendered from the bookmark store,
    // served synchronously — no network request.
    auto network = std::make_unique<CapturingNetwork>();
    auto* network_ptr = network.get();
    ResourceLoader loader(std::move(network), std::make_unique<CapturingNetwork>(), nullptr, nullptr);

    loader.navigate("about:bookmarks");
    (void)loader.consume_pending_updates();  // marks the synchronously-enqueued document ready

    auto view = loader.view("about:bookmarks", ResourceType::Document);
    ASSERT_TRUE(view.has_value());
    EXPECT_EQ(view->state, Hummingbird::Engine::ResourceState::Ready);
    EXPECT_NE(view->body.find("<h1>Bookmarks</h1>"), std::string::npos) << view->body;
    EXPECT_TRUE(network_ptr->requests.empty()) << "about:bookmarks must not hit the network";
}

TEST(ResourceLoaderTest, NeverProbesProviderForOriginRelativeUrls) {
    // The local asset provider must never be probed with page-controlled paths
    // that belong to the document's origin: root-relative ("/x"), protocol-
    // relative ("//host/x"), UNC ("\\host\x"), or absolute-URL links. Feeding
    // such a path to the filesystem is what caused the DDG UNC/SMB stall.
    auto provider = std::make_unique<FakeResourceProvider>();
    auto* provider_ptr = provider.get();

    ResourceLoader loader(std::make_unique<CapturingNetwork>(), std::make_unique<CapturingNetwork>(),
                          std::move(provider), nullptr);

    const std::string base_url = "https://example.dev/page.html";
    loader.request_stylesheets({"//evil.example/x.css", "/rooted.css", "https://evil.example/y.css"}, base_url);
    // Also exercise a base that does not parse as absolute, so a protocol-relative
    // link survives resolution unchanged (the exact UNC-shaped case).
    loader.request_stylesheets({"//evil.example/z.css"}, "");

    for (const auto& id : provider_ptr->queried) {
        ASSERT_FALSE(id.empty());
        EXPECT_NE(id.front(), '/') << "provider probed with origin path: " << id;
        EXPECT_NE(id.front(), '\\') << "provider probed with UNC path: " << id;
        EXPECT_EQ(id.find("://"), std::string::npos) << "provider probed with absolute URL: " << id;
    }
}

TEST(ResourceLoaderTest, CoalescesMultipleArrivalsIntoOneBatch) {
    // Several resources arriving before a tick must drain in a single
    // consume_pending_updates() with one aggregate ready flag, so the tab does
    // one restyle/relayout instead of one per resource (T-PERF-5).
    auto fallback = std::make_unique<CapturingNetwork>();
    fallback->body = "PNGDATA";

    ResourceLoader loader(nullptr, std::move(fallback), nullptr,
                          std::make_unique<Hummingbird::Test::InlineImageDecoder>());
    loader.allow_insecure_host("example.dev");

    const std::string base_url = "https://example.dev/index.html";
    loader.request_images({"/img/a.png", "/img/b.png", "/img/c.png"}, base_url);

    // All three network callbacks fired synchronously and enqueued; a single
    // consume drains them together.
    auto batch = loader.consume_pending_updates();
    EXPECT_EQ(batch.pending_count, 3u);
    EXPECT_TRUE(batch.is_ready(ResourceType::Image));

    // The queue is now empty: no second rebuild is triggered.
    auto empty = loader.consume_pending_updates();
    EXPECT_EQ(empty.pending_count, 0u);
    EXPECT_FALSE(empty.is_ready(ResourceType::Image));
}

TEST(ResourceLoaderTest, ImageUsesFallbackNetworkAndDecoder) {
    auto fallback = std::make_unique<CapturingNetwork>();
    auto* fallback_ptr = fallback.get();
    fallback_ptr->body = "PNGDATA";

    ResourceLoader loader(nullptr, std::move(fallback), nullptr,
                          std::make_unique<Hummingbird::Test::InlineImageDecoder>());
    loader.allow_insecure_host("example.dev");

    const std::string base_url = "https://example.dev/index.html";
    loader.request_images({"/img/logo.png"}, base_url);

    ASSERT_EQ(fallback_ptr->requests.size(), 1u);
    EXPECT_TRUE(fallback_ptr->requests[0].options.allow_insecure);

    auto batch = loader.consume_pending_updates();
    EXPECT_TRUE(batch.is_ready(ResourceType::Image));

    auto resolved = resolve_resource_url(base_url, "/img/logo.png");
    auto view = loader.view(resolved.key, ResourceType::Image);
    ASSERT_TRUE(view.has_value());
    EXPECT_EQ(view->state, Hummingbird::Engine::ResourceState::Ready);
    EXPECT_EQ(view->body, "PNGDATA");
    ASSERT_NE(view->image, nullptr);
    EXPECT_EQ(view->image->width, 2);
    EXPECT_EQ(view->image->height, 2);
}

TEST(ResourceLoaderTest, SvgImagesDecodeThroughCompositeDecoder) {
    auto fallback = std::make_unique<CapturingNetwork>();
    auto* fallback_ptr = fallback.get();
    fallback_ptr->body =
        "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"24\" height=\"12\">"
        "<rect width=\"24\" height=\"12\" fill=\"#000\"/>"
        "</svg>";

    ResourceLoader loader(nullptr, std::move(fallback), nullptr, std::make_unique<CompositeImageDecoder>());

    const std::string base_url = "https://example.dev/index.html";
    loader.request_images({"/img/logo.svg"}, base_url);

    auto batch = loader.consume_pending_updates();
    EXPECT_TRUE(batch.is_ready(ResourceType::Image));

    auto resolved = resolve_resource_url(base_url, "/img/logo.svg");
    auto view = loader.view(resolved.key, ResourceType::Image);
    ASSERT_TRUE(view.has_value());
    ASSERT_NE(view->image, nullptr);
    EXPECT_GT(view->image->width, 0);
    EXPECT_GT(view->image->height, 0);
}

TEST(ResourceLoaderTest, ScriptFetchMarksBatchScriptReady) {
    auto network = std::make_unique<CapturingNetwork>();
    auto* network_ptr = network.get();
    network_ptr->body = "var loaded = true;";

    ResourceLoader loader(std::move(network), std::make_unique<CapturingNetwork>(), nullptr, nullptr);

    const std::string base_url = "https://example.dev/index.html";
    loader.request_scripts({"js/app.js"}, base_url);

    ASSERT_EQ(network_ptr->requests.size(), 1u);
    EXPECT_EQ(network_ptr->requests[0].url, "https://example.dev/js/app.js");

    auto batch = loader.consume_pending_updates();
    EXPECT_TRUE(batch.is_ready(ResourceType::Script));

    auto view = loader.view("https://example.dev/js/app.js", ResourceType::Script);
    ASSERT_TRUE(view.has_value());
    EXPECT_EQ(view->state, Hummingbird::Engine::ResourceState::Ready);
    EXPECT_EQ(view->body, "var loaded = true;");
}

TEST(ResourceLoaderTest, ScriptAssetsMarkReadyWithoutNetwork) {
    auto provider = std::make_unique<FakeResourceProvider>();
    provider->text_["js/app.js"] = "var fromAsset = true;";

    auto network = std::make_unique<CapturingNetwork>();
    auto* network_ptr = network.get();

    ResourceLoader loader(std::move(network), std::make_unique<CapturingNetwork>(), std::move(provider), nullptr);

    const std::string base_url = "https://example.dev/page.html";
    loader.request_scripts({"js/app.js"}, base_url);

    auto resolved = resolve_resource_url(base_url, "js/app.js");
    auto view = loader.view(resolved.key, ResourceType::Script);
    ASSERT_TRUE(view.has_value());
    EXPECT_EQ(view->state, Hummingbird::Engine::ResourceState::Ready);
    EXPECT_EQ(view->body, "var fromAsset = true;");
    EXPECT_TRUE(network_ptr->requests.empty());
}

TEST(ResourceLoaderTest, DemoSubpagesRouteToFallbackNetworkDirectly) {
    // example.dev and all its subpages are the built-in demo site: they must go
    // straight to the stub network instead of failing a real DNS lookup first.
    auto network = std::make_unique<CapturingNetwork>();
    auto* network_ptr = network.get();
    auto fallback = std::make_unique<CapturingNetwork>();
    auto* fallback_ptr = fallback.get();
    fallback_ptr->body = "<html><body>demo</body></html>";

    ResourceLoader loader(std::move(network), std::move(fallback), nullptr, nullptr);
    loader.navigate("https://example.dev/m7");

    EXPECT_TRUE(network_ptr->requests.empty());
    ASSERT_EQ(fallback_ptr->requests.size(), 1u);
    EXPECT_EQ(fallback_ptr->requests[0].url, "https://example.dev/m7");

    auto batch = loader.consume_pending_updates();
    EXPECT_TRUE(batch.is_ready(ResourceType::Document));
}

TEST(ResourceLoaderTest, NavigatePostUsesNetworkPostWithBodyAndContentType) {
    auto network = std::make_unique<CapturingNetwork>();
    auto* network_ptr = network.get();
    network_ptr->body = "<html><body>posted</body></html>";

    ResourceLoader loader(std::move(network), std::make_unique<CapturingNetwork>(), nullptr, nullptr);
    ResourceLoader::DocumentRequest request{};
    request.method = ResourceLoader::DocumentRequest::Method::Post;
    request.body = "q=duck%20duck%20go";
    request.content_type = "application/x-www-form-urlencoded";

    loader.navigate("https://acme.test/html/", request);

    ASSERT_EQ(network_ptr->requests.size(), 1u);
    EXPECT_EQ(network_ptr->requests[0].method, "POST");
    EXPECT_EQ(network_ptr->requests[0].url, "https://acme.test/html/");
    EXPECT_EQ(network_ptr->requests[0].body, "q=duck%20duck%20go");
    EXPECT_EQ(network_ptr->requests[0].options.content_type, "application/x-www-form-urlencoded");

    auto batch = loader.consume_pending_updates();
    EXPECT_TRUE(batch.is_ready(ResourceType::Document));
    EXPECT_EQ(batch.document_url, "https://acme.test/html/");
}

TEST(ResourceLoaderTest, StoresAnimatedImagesWhenDecoderProvidesFrames) {
    auto fallback = std::make_unique<CapturingNetwork>();
    auto* fallback_ptr = fallback.get();
    fallback_ptr->body = "ANIMDATA";

    ResourceLoader loader(nullptr, std::move(fallback), nullptr, std::make_unique<AnimatedImageDecoder>());
    const std::string base_url = "https://example.dev/index.html";
    loader.request_images({"/img/anim.gif"}, base_url);

    auto batch = loader.consume_pending_updates();
    EXPECT_TRUE(batch.is_ready(ResourceType::Image));

    auto resolved = resolve_resource_url(base_url, "/img/anim.gif");
    auto first = loader.view(resolved.key, ResourceType::Image);
    ASSERT_TRUE(first.has_value());
    ASSERT_NE(first->image, nullptr);
    EXPECT_EQ(first->image->pixels[0], 0);

    EXPECT_TRUE(loader.store().tick_animations(110));
    auto second = loader.view(resolved.key, ResourceType::Image);
    ASSERT_TRUE(second.has_value());
    ASSERT_NE(second->image, nullptr);
    EXPECT_EQ(second->image->pixels[0], 1);
}

// --- cookie wiring (8.1.1) ---------------------------------------------------
// The jar itself is covered by CookieJar.test.cpp; these prove it is actually
// attached to real requests, which is the half a jar-only test cannot show.

TEST(ResourceLoaderTest, ServerSetCookieComesBackOnTheNextRequest) {
    auto network = std::make_unique<CapturingNetwork>();
    auto* network_ptr = network.get();
    network_ptr->body = "<html><body>ok</body></html>";
    network_ptr->response_headers.add("Set-Cookie", "session=abc; Path=/");

    auto jar = std::make_shared<Hummingbird::Core::CookieJar>();
    ResourceLoader loader(std::move(network), std::make_unique<CapturingNetwork>(), nullptr, nullptr, jar);

    loader.navigate("https://example.test/login");
    ASSERT_EQ(jar->size(), 1u);
    // The first request could not have carried it yet.
    EXPECT_TRUE(network_ptr->requests[0].options.headers.get("Cookie").empty());

    loader.navigate("https://example.test/account");
    ASSERT_EQ(network_ptr->requests.size(), 2u);
    EXPECT_EQ(network_ptr->requests[1].options.headers.get("Cookie"), "session=abc");
}

TEST(ResourceLoaderTest, CookiesAttachToSubresourceRequestsToo) {
    auto network = std::make_unique<CapturingNetwork>();
    auto* network_ptr = network.get();
    network_ptr->body = "body";

    auto jar = std::make_shared<Hummingbird::Core::CookieJar>();
    jar->store_from_header("https://example.test/", "session=abc", Hummingbird::Core::CookieClock::now());

    ResourceLoader loader(std::move(network), std::make_unique<CapturingNetwork>(), nullptr, nullptr, jar);
    loader.request_stylesheets({"https://example.test/site.css"}, "https://example.test/");

    ASSERT_EQ(network_ptr->requests.size(), 1u);
    EXPECT_EQ(network_ptr->requests[0].options.headers.get("Cookie"), "session=abc");
}

TEST(ResourceLoaderTest, CookiesAttachToFormPostsToo) {
    // The login POST is exactly the request the North Star depends on.
    auto network = std::make_unique<CapturingNetwork>();
    auto* network_ptr = network.get();
    network_ptr->body = "<html>ok</html>";

    auto jar = std::make_shared<Hummingbird::Core::CookieJar>();
    jar->store_from_header("https://example.test/", "csrf=t0ken", Hummingbird::Core::CookieClock::now());

    ResourceLoader loader(std::move(network), std::make_unique<CapturingNetwork>(), nullptr, nullptr, jar);
    ResourceLoader::DocumentRequest request{};
    request.method = ResourceLoader::DocumentRequest::Method::Post;
    request.body = "user=me&pw=secret";
    request.content_type = "application/x-www-form-urlencoded";
    loader.navigate("https://example.test/login", request);

    ASSERT_EQ(network_ptr->requests.size(), 1u);
    EXPECT_EQ(network_ptr->requests[0].method, "POST");
    EXPECT_EQ(network_ptr->requests[0].body, "user=me&pw=secret");
    EXPECT_EQ(network_ptr->requests[0].options.headers.get("Cookie"), "csrf=t0ken");
}

TEST(ResourceLoaderTest, CookiesAreNotSentToADifferentSite) {
    auto network = std::make_unique<CapturingNetwork>();
    auto* network_ptr = network.get();
    network_ptr->body = "body";

    auto jar = std::make_shared<Hummingbird::Core::CookieJar>();
    jar->store_from_header("https://example.test/", "session=abc", Hummingbird::Core::CookieClock::now());

    ResourceLoader loader(std::move(network), std::make_unique<CapturingNetwork>(), nullptr, nullptr, jar);
    loader.request_stylesheets({"https://cdn.other-site.test/x.css"}, "https://example.test/");

    ASSERT_EQ(network_ptr->requests.size(), 1u);
    EXPECT_TRUE(network_ptr->requests[0].options.headers.get("Cookie").empty());
}

TEST(ResourceLoaderTest, WithoutAJarNoCookieHeaderIsEverAdded) {
    auto network = std::make_unique<CapturingNetwork>();
    auto* network_ptr = network.get();
    network_ptr->body = "body";
    network_ptr->response_headers.add("Set-Cookie", "session=abc");

    // Null jar: cookies disabled, and a Set-Cookie response must not crash.
    ResourceLoader loader(std::move(network), std::make_unique<CapturingNetwork>(), nullptr, nullptr, nullptr);
    loader.navigate("https://example.test/login");
    loader.navigate("https://example.test/account");

    ASSERT_EQ(network_ptr->requests.size(), 2u);
    EXPECT_TRUE(network_ptr->requests[1].options.headers.get("Cookie").empty());
}

// The composition the GUI demo actually exercises: the real StubNetwork route,
// the real jar, and the real loader. The tests above use fakes on one side or
// the other; this one has none, so it is what proves example.dev/cookies works.
TEST(ResourceLoaderTest, CookieDemoCounterAdvancesAcrossRealNavigations) {
    auto jar = std::make_shared<Hummingbird::Core::CookieJar>();
    ResourceLoader loader(std::make_unique<CapturingNetwork>(), std::make_unique<Hummingbird::Platform::StubNetwork>(),
                          nullptr, nullptr, jar);

    // StubNetwork answers on a worker thread, so wait for the document to land.
    const auto navigate_and_read = [&](const char* url) {
        loader.navigate(url);
        for (int attempt = 0; attempt < 200; ++attempt) {
            auto batch = loader.consume_pending_updates();
            if (batch.is_ready(ResourceType::Document)) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        auto view = loader.view(url, ResourceType::Document);
        return view ? std::string(view->body) : std::string{};
    };

    const std::string first = navigate_and_read("https://example.dev/cookies");
    EXPECT_NE(first.find("<strong>1</strong>"), std::string::npos) << first;
    // The jar took the counter off the response...
    EXPECT_FALSE(jar->cookie_header_for("https://example.dev/cookies", Hummingbird::Core::CookieClock::now()).empty());

    const std::string second = navigate_and_read("https://example.dev/cookies");
    // ...and sent it back, so the stub could count a second visit.
    EXPECT_NE(second.find("<strong>2</strong>"), std::string::npos) << second;
    EXPECT_NE(second.find("hb_visits=1"), std::string::npos) << "the echoed Cookie header should show what was sent";
}
