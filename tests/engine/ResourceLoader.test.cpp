#include "engine/resources/ResourceLoader.h"

#include <gtest/gtest.h>

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

#include "engine/ResourceUrl.h"
#include "test_utils/TestFakes.h"

namespace {
using Hummingbird::NetworkError;
using Hummingbird::NetworkRequestOptions;
using Hummingbird::NetworkResponse;
using Hummingbird::Engine::resolve_resource_url;
using Hummingbird::Engine::ResourceLoader;
using Hummingbird::Engine::ResourceType;

class FakeResourceProvider final : public Hummingbird::IResourceProvider {
public:
    std::optional<std::string> load_text(std::string_view resource_id) override {
        auto it = text_.find(std::string(resource_id));
        if (it == text_.end()) return std::nullopt;
        return it->second;
    }

    std::optional<std::string> load_bytes(std::string_view resource_id) override {
        auto it = bytes_.find(std::string(resource_id));
        if (it == bytes_.end()) return std::nullopt;
        return it->second;
    }

    std::unordered_map<std::string, std::string> text_;
    std::unordered_map<std::string, std::string> bytes_;
};

class CapturingNetwork final : public Hummingbird::INetwork {
public:
    void get(const std::string& url, std::function<void(NetworkResponse)> callback,
             const NetworkRequestOptions& options = {}) override {
        requests.emplace_back(Request{url, options});
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
        NetworkRequestOptions options;
    };

    std::vector<Request> requests;
    std::string body;
    NetworkError error = NetworkError::None;
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
