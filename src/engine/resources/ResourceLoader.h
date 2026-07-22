#pragma once

#include <stddef.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/net/CookieJar.h"
#include "core/net/IdentityPolicyStore.h"
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
        // Host of the document that initiated this navigation (a link click or
        // form submit). Empty means user-initiated — address bar, bookmark, or
        // history — which is not a cross-site request. Drives SameSite (8.1.2).
        std::string initiator_host;
        // Full URL of the initiating document, for the Referer header. Empty for
        // a user-initiated navigation, which carries no referrer.
        std::string initiator_url;
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

    // `cookie_jar` and `identity_store` are shared across every tab of a profile
    // (see TabManager): a per-loader jar would mean logging in on one tab left
    // another logged out. Null disables that feature — which is what most unit
    // tests want (a null identity store means Transparent identity everywhere).
    ResourceLoader(NetworkPtr network, NetworkPtr fallback_network, ResourceProviderPtr resource_provider,
                   ImageDecoderPtr image_decoder, std::shared_ptr<Core::CookieJar> cookie_jar = nullptr,
                   std::shared_ptr<Core::IdentityPolicyStore> identity_store = nullptr);

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

    Core::CookieJar* cookie_jar() const { return cookie_jar_.get(); }

private:
    // State carried across the hops of one redirect chain (story 8.3.1).
    struct RedirectChain {
        int hops = 0;
        // Every URL visited so far, so an A->B->A cycle is reported as a loop
        // rather than silently burning the hop budget.
        std::vector<std::string> visited;
        // The initiating document's URL, constant across the whole chain. The
        // Referer header is recomputed per hop from this against each hop's URL,
        // so a chain that crosses origins re-applies the referrer policy at every
        // hop instead of forwarding a stale header (parallels the Cookie recompute).
        std::string referrer_source;
    };

    // The single choke point for network requests. `send_request`:
    //   - attaches the jar's Cookie header for `url`, recomputed per hop;
    //   - wraps `callback` so the response's Set-Cookie headers land in the jar
    //     before anything else sees the response;
    //   - follows redirects itself, applying hop/loop limits and RFC 9110
    //     method semantics.
    // Every request (document, subresource, POST, and the stub fallbacks) goes
    // through it, so no call site can silently skip any of that. `context` tells
    // the jar who is asking, which SameSite needs (8.1.2).
    //
    // `post_body` is by value, not a pointer: a redirect re-issues the request
    // from a network callback, long after the caller's body has gone out of
    // scope.
    void send_request(INetwork& network, const std::string& url, NetworkRequestOptions options,
                      std::function<void(NetworkResponse)> callback, const Core::CookieRequestContext& context,
                      std::optional<std::string> post_body = std::nullopt, RedirectChain chain = {});

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
    // Shared per profile. Network callbacks arrive on backend threads, so every
    // touch is guarded by cookie_mutex_.
    std::shared_ptr<Core::CookieJar> cookie_jar_;
    std::shared_ptr<Core::IdentityPolicyStore> identity_store_;
    mutable std::mutex cookie_mutex_;
    ResourceProviderPtr resource_provider_;
    ImageDecoderPtr image_decoder_;
    ResourceStore resource_store_;
    ResourceSecurityPolicy security_policy_;
};

}  // namespace Hummingbird::Engine
