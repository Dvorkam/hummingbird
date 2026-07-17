#include "engine/resources/ResourceLoader.h"

#include <gtest/gtest.h>

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

#include "engine/resources/ResourceUrl.h"
#include "platform/decoders/CompositeImageDecoder.h"
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
    EXPECT_TRUE(batch.image_ready);

    // The queue is now empty: no second rebuild is triggered.
    auto empty = loader.consume_pending_updates();
    EXPECT_EQ(empty.pending_count, 0u);
    EXPECT_FALSE(empty.image_ready);
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
    EXPECT_TRUE(batch.image_ready);

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
    EXPECT_TRUE(batch.image_ready);

    auto resolved = resolve_resource_url(base_url, "/img/logo.svg");
    auto view = loader.view(resolved.key, ResourceType::Image);
    ASSERT_TRUE(view.has_value());
    ASSERT_NE(view->image, nullptr);
    EXPECT_GT(view->image->width, 0);
    EXPECT_GT(view->image->height, 0);
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

    loader.navigate("https://example.dev/html/", request);

    ASSERT_EQ(network_ptr->requests.size(), 1u);
    EXPECT_EQ(network_ptr->requests[0].method, "POST");
    EXPECT_EQ(network_ptr->requests[0].url, "https://example.dev/html/");
    EXPECT_EQ(network_ptr->requests[0].body, "q=duck%20duck%20go");
    EXPECT_EQ(network_ptr->requests[0].options.content_type, "application/x-www-form-urlencoded");

    auto batch = loader.consume_pending_updates();
    EXPECT_TRUE(batch.document_ready);
    EXPECT_EQ(batch.document_url, "https://example.dev/html/");
}

TEST(ResourceLoaderTest, StoresAnimatedImagesWhenDecoderProvidesFrames) {
    auto fallback = std::make_unique<CapturingNetwork>();
    auto* fallback_ptr = fallback.get();
    fallback_ptr->body = "ANIMDATA";

    ResourceLoader loader(nullptr, std::move(fallback), nullptr, std::make_unique<AnimatedImageDecoder>());
    const std::string base_url = "https://example.dev/index.html";
    loader.request_images({"/img/anim.gif"}, base_url);

    auto batch = loader.consume_pending_updates();
    EXPECT_TRUE(batch.image_ready);

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
