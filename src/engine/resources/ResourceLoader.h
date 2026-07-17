#pragma once

#include <stddef.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/platform_api/IImageDecoder.h"
#include "core/platform_api/INetwork.h"
#include "core/platform_api/IResourceProvider.h"
#include "engine/resources/ResourceSecurityPolicy.h"
#include "engine/resources/ResourceStore.h"

namespace Hummingbird::Engine {
namespace ResourceRequestPlanning {
struct ResourceRequestOptions;
}

class ResourceLoader {
public:
    struct DocumentRequest {
        enum class Method {
            Get,
            Post,
        };

        Method method = Method::Get;
        std::string body;
        std::string content_type;
    };

    struct PendingResourceUpdate {
        ResourceType type;
        std::string url;
        std::string effective_url;
        std::string body;
        bool success;
        NetworkError error = NetworkError::None;
    };

    struct BatchResult {
        std::array<bool, kResourceTypeCount> ready{};
        size_t pending_count = 0;
        std::string document_url;
        std::string effective_url;
        NetworkError document_error = NetworkError::None;

        bool is_ready(ResourceType type) const { return ready[static_cast<size_t>(type)]; }
    };

    ResourceLoader(NetworkPtr network, NetworkPtr fallback_network, ResourceProviderPtr resource_provider,
                   ImageDecoderPtr image_decoder);

    ResourceLoader(const ResourceLoader&) = delete;
    ResourceLoader& operator=(const ResourceLoader&) = delete;
    ResourceLoader(ResourceLoader&&) = delete;
    ResourceLoader& operator=(ResourceLoader&&) = delete;

    void shutdown();
    void reset();
    void navigate(std::string_view url);
    void navigate(std::string_view url, const DocumentRequest& request);
    void allow_insecure_host(std::string_view host);
    bool is_insecure_allowed_for_url(std::string_view url) const;

    void request_stylesheets(const std::vector<std::string>& links, std::string_view base_url);
    void request_images(const std::vector<std::string>& links, std::string_view base_url);
    void request_fonts(const std::vector<std::string>& links, std::string_view base_url);
    void request_scripts(const std::vector<std::string>& links, std::string_view base_url);

    BatchResult consume_pending_updates();

    std::optional<ResourceView> view(std::string_view url, ResourceType type) const;
    const ResourceEntry* find(std::string_view url, ResourceType type) const;
    ResourceStore& store() { return resource_store_; }
    const ResourceStore& store() const { return resource_store_; }
    IResourceProvider* resource_provider() const { return resource_provider_.get(); }
    IImageDecoder* image_decoder() const { return image_decoder_.get(); }

private:
    void request_resources(const std::vector<std::string>& links, std::string_view base_url,
                           const ResourceRequestPlanning::ResourceRequestOptions& options);
    void enqueue_resource_update(ResourceType type, std::string url, std::string body, bool success,
                                 std::string effective_url = {}, NetworkError error = NetworkError::None);
    std::vector<PendingResourceUpdate> take_pending_resources();

    std::atomic<uint64_t> nav_counter_{0};
    std::atomic<uint64_t> active_nav_{0};

    std::mutex pending_mutex_;
    std::vector<PendingResourceUpdate> pending_resources_;

    NetworkPtr network_;
    NetworkPtr fallback_network_;
    ResourceProviderPtr resource_provider_;
    ImageDecoderPtr image_decoder_;
    ResourceStore resource_store_;
    ResourceSecurityPolicy security_policy_;
};

}  // namespace Hummingbird::Engine
