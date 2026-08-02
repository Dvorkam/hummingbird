#pragma once

#include <stddef.h>

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/net/CookieJar.h"
#include "core/net/HttpCache.h"
#include "core/net/IdentityPolicyStore.h"
#include "core/net/RequestFilter.h"
#include "core/platform_api/IImageDecoder.h"
#include "core/platform_api/INetwork.h"
#include "core/platform_api/IResourceProvider.h"
#include "core/platform_api/ScriptFetch.h"
#include "engine/resources/ResourceSecurityPolicy.h"
#include "engine/resources/ResourceStore.h"

namespace Hummingbird::Engine {
namespace ResourceRequestPlanning {
struct ResourceRequestOptions;
}

class ResourceLoader {
public:
    // How long a request may take, in milliseconds (story 9.1.3). `total_ms`
    // bounds the WHOLE request including every redirect hop, which is the point:
    // the transport can only see one hop at a time, so a per-call limit alone
    // lets a chain multiply it by the hop budget.
    struct RequestDeadlines {
        long connect_ms = 5000;
        long total_ms = 15000;
    };
    void set_request_deadlines(RequestDeadlines deadlines) { deadlines_ = deadlines; }
    RequestDeadlines request_deadlines() const { return deadlines_; }

    // The clock the deadline is measured against. Injectable so a test can prove
    // a chain gives up part-way without sleeping through a real budget — the
    // same reason the cookie code takes `now` as a parameter everywhere.
    using SteadyClock = std::function<std::chrono::steady_clock::time_point()>;
    void set_deadline_clock(SteadyClock clock) { clock_ = std::move(clock); }

    // How a request may use the HTTP cache (story 9.3.1).
    //
    // This exists because the cache would otherwise break reload: a page with
    // `max-age=3600` would serve from memory for an hour with no way to refresh
    // it. The two levels mirror what browsers actually do, which is not the same
    // thing at both levels — see `navigate` for how they propagate.
    enum class CachePolicy {
        // Ordinary navigation: a fresh entry is served without asking.
        Default,
        // Normal reload (F5 / Ctrl+R): the entry must be confirmed with the
        // server before reuse. Still conditional, so an unchanged resource costs
        // a round trip and no body. Chrome sends `Cache-Control: max-age=0` for
        // exactly this.
        Revalidate,
        // Hard reload (Ctrl+Shift+R / Ctrl+F5): do not READ the cache at all, not
        // even conditionally. The fresh response is still stored, so this
        // refreshes the entry rather than disabling caching for the page.
        Bypass,
    };

    struct DocumentRequest {
        enum class Method {
            Get,
            Post,
        };

        Method method = Method::Get;
        // Set by a reload. Applies to the document; what the subresources inherit
        // depends on which kind of reload it is (see `navigate`).
        CachePolicy cache_policy = CachePolicy::Default;
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

    // `cookie_jar`, `identity_store` and `http_cache` are shared across every tab
    // of a profile (see TabManager): a per-loader jar would mean logging in on
    // one tab left another logged out, and a per-loader cache would refetch what
    // the tab next door already has. Null disables that feature — which is what
    // most unit tests want (a null identity store means Transparent identity
    // everywhere; a null cache means every request goes to the network, which is
    // what a test asserting on request counts expects unless it says otherwise).
    ResourceLoader(NetworkPtr network, NetworkPtr fallback_network, ResourceProviderPtr resource_provider,
                   ImageDecoderPtr image_decoder, std::shared_ptr<Core::CookieJar> cookie_jar = nullptr,
                   std::shared_ptr<Core::IdentityPolicyStore> identity_store = nullptr,
                   std::shared_ptr<Core::HttpCache> http_cache = nullptr,
                   std::shared_ptr<Core::RequestFilter> request_filter = nullptr);

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

    // Issues a script-initiated request (story 9.1.1) through the same choke
    // point as everything else, so it inherits cookies, per-origin identity,
    // referrer policy, redirect handling and the 9.1.3 deadline. It deliberately
    // does NOT go through the resource store: a fetch result belongs to the page's
    // JavaScript, not to the document's resource set, and caching it as a
    // subresource would let a later navigation serve it as one.
    //
    // `callback` runs on whatever thread the transport answers on, so the caller
    // must marshal to the main thread before touching the script engine.
    void fetch_for_script(const ScriptFetchRequest& request, std::string_view document_url,
                          std::function<void(ScriptFetchResponse)> callback);

    // Ends the window in which a script fetch inherits the navigation's cache
    // policy (T-NET-RELOAD-FETCH-POLICY-1). Called by the Tab once the
    // document's own scripts have run: a fetch issued from a timer or a promise
    // continuation afterwards is not part of the load the user reloaded.
    void end_navigation_script_phase();

    BatchResult consume_pending_updates();

    std::optional<ResourceView> view(std::string_view url, ResourceType type) const;
    const ResourceEntry* find(std::string_view url, ResourceType type) const;
    ResourceStore& store() { return resource_store_; }
    const ResourceStore& store() const { return resource_store_; }
    IResourceProvider* resource_provider() const { return resource_provider_.get(); }
    IImageDecoder* image_decoder() const { return image_decoder_.get(); }

    Core::CookieJar* cookie_jar() const { return cookie_jar_.get(); }
    Core::HttpCache* http_cache() const { return http_cache_.get(); }

private:
    // State carried across the hops of one redirect chain (story 8.3.1).
    struct RedirectChain {
        int hops = 0;
        // HTTP method for the current hop. Explicit state rather than inferred
        // from body presence: OPTIONS and bodyless POST are both meaningful,
        // and redirects may rewrite POST to GET while preserving PUT/DELETE.
        std::string method = "GET";
        // When the whole chain must be done, set on the first hop and carried
        // unchanged through every later one (story 9.1.3). This is what makes
        // the budget belong to the REQUEST rather than to each hop: without it,
        // a 20-hop chain could spend the per-call limit twenty times over.
        // Default-constructed (epoch) means "not started yet".
        std::chrono::steady_clock::time_point deadline{};
        // Every URL visited so far, so an A->B->A cycle is reported as a loop
        // rather than silently burning the hop budget.
        std::vector<std::string> visited;
        // The initiating document's URL, constant across the whole chain. The
        // Referer header is recomputed per hop from this against each hop's URL,
        // so a chain that crosses origins re-applies the referrer policy at every
        // hop instead of forwarding a stale header (parallels the Cookie recompute).
        std::string referrer_source;

        // CORS state for the chain (story 9.2.3). Carried per hop for the same
        // reason the Cookie header is: a chain can cross origins, and a check
        // applied only to the first request is not a check at all — the classic
        // way a strict CORS implementation turns out not to be strict.
        struct CorsState {
            // Empty unless this is a script-initiated fetch; navigations and
            // subresource loads are not subject to CORS in M9.
            std::string document_url;
            std::string origin;
            Core::Cors::Credentials credentials = Core::Cors::Credentials::SameOrigin;
            // Set once any hop is cross-origin, and never cleared: a chain that
            // wanders off-origin and comes back is still cross-origin, or a
            // server could launder access by bouncing through the initiator.
            bool active = false;
            // Set once a cross-origin redirect has been followed. A tainted
            // request presents `Origin: null`, so the server must opt in to an
            // opaque origin rather than to the page that started it.
            bool tainted = false;
            // A preflighted request may not follow redirects: the server agreed
            // to the request it was asked about, not to wherever it points next.
            bool preflighted = false;
            // True for the preflight itself, which may not redirect either.
            bool is_preflight = false;

            bool enabled() const { return !document_url.empty(); }
            // What the server must name in Access-Control-Allow-Origin.
            std::string effective_origin() const { return tainted ? std::string("null") : origin; }
        };
        CorsState cors;

        // How this chain may use the cache (story 9.3.1). Carried here rather
        // than read off a member at response time because a chain outlives the
        // call that started it — a redirect hop issued from a network callback
        // must use the policy the navigation asked for, not whatever the next
        // navigation has since set.
        CachePolicy cache_policy = CachePolicy::Default;
        // Set once a 304 arrived for an entry that was no longer there, so the
        // unconditional retry cannot itself retry and loop.
        bool revalidation_retried = false;

        // What this request is FOR, for declarative filtering (story 9.4.1).
        // Carried on the chain rather than passed per call because it is a
        // property of the request, constant across every hop — an image that
        // redirects is still an image.
        //
        // Defaulting to Document means "never filtered" (a top-level navigation
        // is out of M9 scope), so a new call site that forgets to set it
        // fails OPEN. For a blocker that is the right direction to fail: an
        // unblocked ad is a nuisance, a wrongly blocked resource is a broken
        // page with no explanation.
        Core::RequestDestination destination = Core::RequestDestination::Document;
        // The document that initiated the request, immutable across redirects.
        // Cookie SameSite context evolves per hop; third-party filtering must
        // always remain relative to the original document.
        std::string filter_initiator_host;
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
    // `request_body` is by value, not a pointer: a redirect re-issues the request
    // from a network callback, long after the caller's body has gone out of
    // scope.
    // No default arguments here: `chain` is a nested aggregate with default member
    // initializers, and the standard forbids using those in a default argument of
    // the enclosing class's own member (GCC enforces this; MSVC does not). Every
    // caller passes `request_body` (or std::nullopt) and a `RedirectChain` explicitly.
    void send_request(INetwork& network, const std::string& url, NetworkRequestOptions options,
                      std::function<void(NetworkResponse)> callback, const Core::CookieRequestContext& context,
                      std::optional<std::string> request_body, RedirectChain chain);

    // Starts `chain`'s deadline if this is its first hop, then writes what is
    // left of the budget into `options`. Returns false when the budget is
    // already spent, in which case the caller must fail the request rather than
    // issue a hop that could not finish in time.
    // CORS preflight (story 9.2.1): an OPTIONS request asking whether the real
    // request may be made at all. Calls `done(true)` only when the server
    // approved the method and every non-safelisted header. Never credentialed —
    // "may I" must not depend on who is logged in.
    void send_preflight(INetwork& network, const ScriptFetchRequest& request, const NetworkRequestOptions& options,
                        const std::string& origin, const Core::CookieRequestContext& context,
                        const RedirectChain& chain, std::function<void(bool)> done);

    bool apply_deadline(RedirectChain& chain, NetworkRequestOptions& options) const;
    std::chrono::steady_clock::time_point now() const;

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
    // Self-synchronizing, so unlike the jar it needs no lock here.
    std::shared_ptr<Core::HttpCache> http_cache_;
    // Declarative block rules, shared per profile and populated by the extension
    // host (story 9.4.1). Null means nothing is filtered, which is what every
    // test that does not say otherwise wants. Self-synchronizing, like the cache.
    std::shared_ptr<Core::RequestFilter> request_filter_;
    mutable std::mutex cookie_mutex_;

    // The cache policy the subresources of the navigation in flight inherit.
    // NOT simply a copy of the document's: a normal reload revalidates the
    // document only, which is what browsers settled on after finding that
    // re-checking every subresource made reload slow enough to be avoided.
    std::atomic<CachePolicy> nav_cache_policy_{CachePolicy::Default};

    // The policy a script-initiated fetch inherits while the navigation's own
    // scripts are still running (T-NET-RELOAD-FETCH-POLICY-1).
    //
    // Separate from `nav_cache_policy_` because it is armed and DISARMED at a
    // different moment. A page whose script fetches its data on load — every
    // SPA, and M9's own proof target — otherwise served that data from cache on
    // a hard reload, i.e. the one gesture that is supposed to guarantee
    // freshness refreshed the document and not the content the user came for.
    //
    // Reset to Default by `end_navigation_script_phase`, because a reload has no
    // business governing a fetch the page fires a minute later.
    std::atomic<CachePolicy> script_fetch_policy_{CachePolicy::Default};

    RequestDeadlines deadlines_;
    // Null means the real steady clock; see set_deadline_clock.
    SteadyClock clock_;
    ResourceProviderPtr resource_provider_;
    ImageDecoderPtr image_decoder_;
    ResourceStore resource_store_;
    ResourceSecurityPolicy security_policy_;
};

}  // namespace Hummingbird::Engine
