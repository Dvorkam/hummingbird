#pragma once

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
#include "engine/ResourceStore.h"

namespace Hummingbird::Engine {

class ResourceLoader {
public:
    struct PendingResourceUpdate {
        ResourceType type;
        std::string url;
        std::string effective_url;
        std::string body;
        bool success;
    };

    struct BatchResult {
        bool document_ready = false;
        bool stylesheet_ready = false;
        bool image_ready = false;
        size_t pending_count = 0;
        std::string document_url;
        std::string effective_url;
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

    void request_stylesheets(const std::vector<std::string>& links, std::string_view base_url);
    void request_images(const std::vector<std::string>& links, std::string_view base_url);

    BatchResult consume_pending_updates();

    std::optional<ResourceView> view(std::string_view url, ResourceType type) const;
    const ResourceEntry* find(std::string_view url, ResourceType type) const;
    ResourceStore& store() { return resource_store_; }
    const ResourceStore& store() const { return resource_store_; }
    IResourceProvider* resource_provider() const { return resource_provider_.get(); }

private:
    struct ResourceRequestOptions {
        ResourceType type;
        std::string_view type_label;
        std::string_view attr_label;
        bool allow_fallback_network;
        bool log_duplicates;
        bool log_asset_load;
        bool mark_ready_on_asset;
        bool use_binary;
    };

    void request_resources(const std::vector<std::string>& links, std::string_view base_url,
                           const ResourceRequestOptions& options);
    void enqueue_resource_update(ResourceType type, std::string url, std::string body, bool success,
                                 std::string effective_url = {});
    std::vector<PendingResourceUpdate> take_pending_resources();
    void handle_document_update(PendingResourceUpdate& update, BatchResult& result, bool& document_ready);
    void handle_stylesheet_update(PendingResourceUpdate& update, bool& stylesheet_ready);
    void handle_image_update(PendingResourceUpdate& update, bool& image_ready, size_t& image_decode_count,
                             double& image_decode_ms);
    void handle_failed_update(const PendingResourceUpdate& update);

    std::atomic<uint64_t> nav_counter_{0};
    std::atomic<uint64_t> active_nav_{0};

    std::mutex pending_mutex_;
    std::vector<PendingResourceUpdate> pending_resources_;

    NetworkPtr network_;
    NetworkPtr fallback_network_;
    ResourceProviderPtr resource_provider_;
    ImageDecoderPtr image_decoder_;
    ResourceStore resource_store_;
};

}  // namespace Hummingbird::Engine
