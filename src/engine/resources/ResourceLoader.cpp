#include "engine/resources/ResourceLoader.h"

#include <algorithm>
#include <chrono>
#include <functional>
#include <optional>
#include <ostream>
#include <utility>

#include "core/BookmarkStore.h"
#include "core/net/Referrer.h"
#include "core/utils/DataUrl.h"
#include "core/utils/Log.h"
#include "core/utils/Timing.h"
#include "core/utils/Url.h"
#include "engine/resources/NetworkErrorPage.h"
#include "engine/resources/RedirectPolicy.h"
#include "engine/resources/ResourceRequestPlanner.h"
#include "engine/resources/ResourceUpdateProcessor.h"

namespace Hummingbird::Engine {

namespace {
// The built-in demo site: example.dev and every subpage under it are served by
// the stub network. Routing them straight there keeps startup deterministic
// and avoids a doomed DNS lookup (and its scary warning) per demo navigation.
bool is_builtin_demo_url(std::string_view url) {
    // `ads.example.net` is the demo site's THIRD-PARTY origin (story 9.4.2). It
    // has to be routed here too, or the ad-block demo's tracker requests leave
    // for real DNS and fail — which would look like blocking and prove nothing.
    for (std::string_view prefix :
         {"http://example.dev", "https://example.dev", "http://ads.example.net", "https://ads.example.net"}) {
        if (url.rfind(prefix, 0) != 0) continue;
        if (url.size() == prefix.size()) return true;
        const char next = url[prefix.size()];
        if (next == '/' || next == '?') return true;
    }
    return false;
}

// The cookie context for a top-level document navigation (8.1.2).
//
// `initiator_host` is the document that caused the navigation — a link click or
// form submit. Empty means user-initiated (address bar, bookmark, history),
// which is not a cross-site request and carries every cookie. Together with
// safe_method this is what makes SameSite=Lax refuse a cross-site form POST,
// the CSRF case Lax exists to block (T-COOKIE-NAV-INITIATOR-1).
Core::CookieRequestContext document_cookie_context(bool is_post, std::string initiator_host) {
    Core::CookieRequestContext context;
    context.top_level_navigation = true;
    context.safe_method = !is_post;
    context.initiator_host = std::move(initiator_host);
    return context;
}

// Subresources DO know their initiator: the document that referenced them. This
// is the case the 8.1.2 acceptance criterion names, and it is exact.
Core::CookieRequestContext subresource_cookie_context(std::string_view base_url) {
    Core::CookieRequestContext context;
    context.top_level_navigation = false;
    context.safe_method = true;
    if (auto parts = Core::parse_absolute_url(base_url)) {
        context.initiator_host = parts->host;
    }
    return context;
}

// The one place that translates the engine's resource-store vocabulary into the
// filter's request-purpose vocabulary (story 9.4.1). They are kept separate so
// `core/` never has to know what a resource store is; this is the single seam
// where both are in scope.
//
// A `switch` with no `default`, deliberately: adding a ResourceType must break
// this build rather than silently map to Document, which is the one value that
// is never filtered. A new resource type quietly becoming unblockable is a hole
// nobody would notice.
Core::RequestDestination filter_destination_for(ResourceType type) {
    switch (type) {
        case ResourceType::Document:
            return Core::RequestDestination::Document;
        case ResourceType::Stylesheet:
            return Core::RequestDestination::Stylesheet;
        case ResourceType::Image:
            return Core::RequestDestination::Image;
        case ResourceType::Font:
            return Core::RequestDestination::Font;
        case ResourceType::Script:
            return Core::RequestDestination::Script;
        case ResourceType::Count:
            break;
    }
    return Core::RequestDestination::Document;
}
}  // namespace

ResourceLoader::ResourceLoader(NetworkPtr network, NetworkPtr fallback_network, ResourceProviderPtr resource_provider,
                               ImageDecoderPtr image_decoder, std::shared_ptr<Core::CookieJar> cookie_jar,
                               std::shared_ptr<Core::IdentityPolicyStore> identity_store,
                               std::shared_ptr<Core::HttpCache> http_cache,
                               std::shared_ptr<Core::RequestFilter> request_filter)
    : network_(std::move(network)),
      fallback_network_(std::move(fallback_network)),
      resource_provider_(std::move(resource_provider)),
      image_decoder_(std::move(image_decoder)),
      cookie_jar_(std::move(cookie_jar)),
      identity_store_(std::move(identity_store)),
      http_cache_(std::move(http_cache)),
      request_filter_(std::move(request_filter)) {
    // Only "nothing can fetch at all" is an error. Having just one backend is a
    // deliberate configuration (tests, headless harnesses, stub-only demo runs),
    // and shouting about it on every construction trained the eye to ignore a
    // line that should mean something.
    if (!network_ && !fallback_network_) {
        HB_LOG_ERROR("[network] no network backend available; requests will fail");
    } else if (!network_ || !fallback_network_) {
        HB_LOG_DEBUG("[network] single backend configured (primary=" << (network_ ? "yes" : "no") << " fallback="
                                                                     << (fallback_network_ ? "yes" : "no") << ")");
    }
    // Both are optional collaborators, and a loader constructed without them is a
    // valid configuration rather than a fault. Warning here fires once per
    // construction regardless of whether anything ever needed them; the paths
    // that DO need them already warn at the point of use, which is where the
    // absence actually costs something.
    if (!resource_provider_) {
        HB_LOG_DEBUG("[resource] constructed without a resource provider");
    }
    if (!image_decoder_) {
        HB_LOG_DEBUG("[image] constructed without a decoder");
    }
}

void ResourceLoader::fetch_for_script(const ScriptFetchRequest& request, std::string_view document_url,
                                      std::function<void(ScriptFetchResponse)> callback) {
    // Pick the transport the same way a document navigation does: the built-in
    // demo site is served by the stub, which has no DNS behind it. Without this a
    // fetch to example.dev went to the real network and failed to resolve, while
    // the page around it had loaded fine — the two took different routes to the
    // same host.
    INetwork* transport = network_.get();
    if (is_builtin_demo_url(request.url) && fallback_network_) {
        transport = fallback_network_.get();
    }

    ScriptFetchResponse failure;
    if (!transport) {
        failure.failure = ScriptFetchFailure::NetworkError;
        if (callback) callback(std::move(failure));
        return;
    }
    // Only http(s) is fetchable. Anything else — file:, data:, a relative URL the
    // host could not resolve — is rejected here rather than handed to a transport
    // that refuses schemes silently.
    auto target = Core::parse_absolute_url(request.url);
    if (!target || (target->scheme != "http" && target->scheme != "https")) {
        failure.failure = ScriptFetchFailure::BadUrl;
        failure.url = request.url;
        if (callback) callback(std::move(failure));
        return;
    }

    NetworkRequestOptions options;
    options.headers = request.headers;
    options.allow_insecure = is_insecure_allowed_for_url(request.url);
    if (request.has_body) {
        // The page picked its own Content-Type if it cared; otherwise fall back
        // to the same default a form POST uses.
        options.content_type = request.headers.get("Content-Type");
    }

    // CORS classification (story 9.2.1). Same-origin requests are not subject to
    // it at all; cross-origin ones must announce who is asking and then have the
    // response vetted before the page may see any part of it.
    const bool cross_origin = !Core::Cors::is_same_origin(request.url, document_url);
    auto document_origin = Core::Origin::parse(document_url);
    const std::string origin_header = document_origin ? document_origin->serialize() : std::string("null");

    // The initiating document is this fetch's referrer source and its SameSite
    // initiator: a fetch is a subresource request, never a top-level navigation,
    // so a cross-site one must not carry Lax cookies.
    Core::CookieRequestContext context;
    context.top_level_navigation = false;
    context.safe_method = request.method == "GET" || request.method == "HEAD";
    if (auto initiator = Core::parse_absolute_url(document_url)) {
        context.initiator_host = initiator->host;
    }
    // Credentials mode decides whether cookies ride along at all. The default is
    // SameOrigin, so a cross-origin fetch is anonymous unless the page opts in —
    // which is why enabling `credentials: 'include'` also raises the bar the
    // server must clear (no `*`, and Allow-Credentials required).
    context.credentials_allowed = request.credentials == Core::Cors::Credentials::Include ||
                                  (request.credentials == Core::Cors::Credentials::SameOrigin && !cross_origin);

    RedirectChain chain;
    chain.referrer_source = std::string(document_url);
    // Enabling CORS for the chain, not just this request: send_request applies
    // it per hop, so a same-origin URL that redirects off-origin is caught too
    // (story 9.2.3).
    chain.cors.document_url = std::string(document_url);
    chain.cors.origin = origin_header;
    chain.cors.credentials = request.credentials;
    chain.method = request.method;
    chain.destination = Core::RequestDestination::Fetch;
    // Without this a script fetch always ran at Default, so a hard reload
    // refreshed the document and its subresources and then served the page's
    // data from cache — the shape of every SPA, including M9's proof target.
    chain.cache_policy = script_fetch_policy_.load(std::memory_order_acquire);

    std::optional<std::string> body;
    if (request.has_body) {
        body = request.body;
    }

    // Translates a transport answer into the page's view of it. The CORS verdict
    // is NOT taken here — send_request applies it per hop, because a check that
    // only sees the final response cannot see the chain that produced it.
    auto deliver = [callback](NetworkResponse response) mutable {
        ScriptFetchResponse out;
        switch (response.error) {
            case NetworkError::None:
                break;
            case NetworkError::Timeout:
                out.failure = ScriptFetchFailure::Timeout;
                break;
            case NetworkError::CorsBlocked:
                out.failure = ScriptFetchFailure::CorsBlocked;
                break;
            default:
                out.failure = ScriptFetchFailure::NetworkError;
                break;
        }
        if (out.failure == ScriptFetchFailure::None && response.status == 0) {
            // Nothing came back at all; a 0-status Response would be a lie.
            out.failure = ScriptFetchFailure::NetworkError;
        }
        // On ANY failure — including a CORS block — nothing is copied across:
        // not the body, not the headers, not even the status. A page that learns
        // "that origin answered 401" has read cross-origin state it was refused.
        if (out.failure == ScriptFetchFailure::None) {
            out.status = response.status;
            out.url = response.effective_url.empty() ? response.url : response.effective_url;
            out.headers = response.headers;
            out.body = std::move(response.body);
        }
        if (callback) callback(std::move(out));
    };

    // A "simple" request is one a plain <form> could already have made, so it
    // goes straight out. Anything else asks permission first: the point of a
    // preflight is that the server never SEES the real request until it has
    // agreed to it, which matters when that request would delete something.
    if (cross_origin && !Core::Cors::is_simple_request(request.method, options.headers)) {
        send_preflight(*transport, request, options, origin_header, context, chain,
                       [this, transport, url = request.url, options, context, chain, body, deliver,
                        callback](bool approved) mutable {
                           if (!approved) {
                               // The real request is never sent. That is the point
                               // of a preflight: a server that has not agreed does
                               // not get to see a DELETE it would have acted on.
                               ScriptFetchResponse blocked;
                               blocked.failure = ScriptFetchFailure::CorsBlocked;
                               if (callback) callback(std::move(blocked));
                               return;
                           }
                           // The approval covers THIS request. If the server now
                           // redirects, the chain stops rather than following
                           // somewhere the preflight never asked about.
                           chain.cors.preflighted = true;
                           send_request(*transport, url, options, deliver, context, std::move(body), std::move(chain));
                       });
        return;
    }

    send_request(*transport, request.url, options, deliver, context, std::move(body), std::move(chain));
}

void ResourceLoader::send_preflight(INetwork& network, const ScriptFetchRequest& request,
                                    const NetworkRequestOptions& options, const std::string& origin,
                                    const Core::CookieRequestContext& context, const RedirectChain& chain,
                                    std::function<void(bool)> done) {
    NetworkRequestOptions preflight;
    preflight.allow_insecure = options.allow_insecure;
    preflight.headers.set("Origin", origin);
    preflight.headers.set("Access-Control-Request-Method", request.method);
    const auto names = Core::Cors::headers_needing_preflight(options.headers);
    if (!names.empty()) {
        std::string joined;
        for (const auto& name : names) {
            if (!joined.empty()) joined += ",";
            joined += name;
        }
        preflight.headers.set("Access-Control-Request-Headers", joined);
    }

    // A preflight is never credentialed, whatever the real request is: it asks
    // "may I", and the answer must not depend on who is logged in.
    Core::CookieRequestContext preflight_context = context;
    preflight_context.credentials_allowed = false;

    RedirectChain preflight_chain;
    preflight_chain.referrer_source = chain.referrer_source;
    preflight_chain.method = "OPTIONS";
    // Marked as a preflight so the redirect loop refuses to follow one, but the
    // per-hop CORS path stays OFF: a preflight's own response is judged by
    // check_preflight (which asks about the method and headers too), and running
    // both would check it twice against different rules.
    preflight_chain.cors.is_preflight = true;
    // Filtered like the request it asks about. A preflight to a blocked endpoint
    // is still a request to that endpoint — and sending one for a request that
    // will be refused anyway would announce the page's intent to a server the
    // user has said they do not want contacted.
    preflight_chain.destination = chain.destination;
    // Carry the deadline, so a slow preflight spends the request's budget rather
    // than doubling it (story 9.1.3).
    preflight_chain.deadline = chain.deadline;

    const std::string url = request.url;
    const std::string method = request.method;
    Core::HttpHeaders real_headers = options.headers;
    auto credentials = request.credentials;
    send_request(
        network, url, preflight,
        [done = std::move(done), origin, credentials, method, real_headers, url](NetworkResponse response) {
            if (response.error != NetworkError::None || response.status == 0) {
                HB_LOG_WARN("[cors] preflight for " << url << " did not complete");
                done(false);
                return;
            }
            const auto decision =
                Core::Cors::check_preflight(response.headers, origin, credentials, method, real_headers);
            if (decision != Core::Cors::Decision::Allowed) {
                HB_LOG_WARN("[cors] preflight refused " << url << " for " << origin << ": "
                                                        << Core::Cors::describe(decision));
                done(false);
                return;
            }
            done(true);
        },
        preflight_context, std::nullopt, std::move(preflight_chain));
}

std::chrono::steady_clock::time_point ResourceLoader::now() const {
    return clock_ ? clock_() : std::chrono::steady_clock::now();
}

bool ResourceLoader::apply_deadline(RedirectChain& chain, NetworkRequestOptions& options) const {
    const auto current = now();
    if (chain.deadline == std::chrono::steady_clock::time_point{}) {
        // First hop: the budget starts here and belongs to the whole chain.
        chain.deadline = current + std::chrono::milliseconds(deadlines_.total_ms);
    }
    const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(chain.deadline - current).count();
    if (remaining <= 0) {
        return false;
    }
    options.total_timeout_ms = static_cast<long>(remaining);
    // Connecting may not outlast what is left of the whole request: a 5s connect
    // budget is meaningless with 300ms left on the chain.
    options.connect_timeout_ms = std::min<long>(deadlines_.connect_ms, static_cast<long>(remaining));
    return true;
}

void ResourceLoader::send_request(INetwork& network, const std::string& url, NetworkRequestOptions options,
                                  std::function<void(NetworkResponse)> callback,
                                  const Core::CookieRequestContext& context, std::optional<std::string> request_body,
                                  RedirectChain chain) {
    // Declarative request filtering (story 9.4.1). First thing in the function,
    // so a blocked request costs no cookie header, no cache lookup and no
    // transport — it is refused before anything is spent on it.
    //
    // Being here also means it runs on EVERY HOP, which is the point rather than
    // a side effect: the CORS code below makes the same argument, and it applies
    // with more force here because redirecting through a chain of domains is
    // ordinary behaviour for ad and tracking networks. Filtering only the first
    // URL would be trivially evaded by a single 302.
    if (request_filter_) {
        const auto verdict = request_filter_->match({url, chain.destination, context.initiator_host});
        if (verdict.blocked) {
            HB_LOG_INFO("[filter] blocked " << url << " (" << verdict.source << " rule " << verdict.rule_id << ")");
            NetworkResponse blocked;
            blocked.url = chain.visited.empty() ? url : chain.visited.front();
            blocked.effective_url = url;
            blocked.error = NetworkError::BlockedByFilter;
            if (callback) callback(std::move(blocked));
            return;
        }
    }

    // Spend the chain's remaining time budget on this hop (story 9.1.3). A hop
    // that starts with nothing left fails as a timeout instead of being issued:
    // the transport only ever sees one hop, so nothing below can enforce a limit
    // that spans the chain.
    if (!apply_deadline(chain, options)) {
        HB_LOG_WARN("[network] request deadline exceeded after " << chain.hops << " hops: " << url);
        NetworkResponse timed_out;
        timed_out.url = chain.visited.empty() ? url : chain.visited.front();
        timed_out.effective_url = url;
        timed_out.error = NetworkError::Timeout;
        if (callback) callback(std::move(timed_out));
        return;
    }

    // Recomputed per hop: a chain can cross hosts and paths, so the previous
    // hop's Cookie header must never ride along.
    if (cookie_jar_ && context.credentials_allowed) {
        std::lock_guard<std::mutex> lg(cookie_mutex_);
        const std::string cookies = cookie_jar_->cookie_header_for(url, Core::CookieClock::now(), context);
        if (cookies.empty()) {
            options.headers.remove("Cookie");
        } else {
            options.headers.set("Cookie", cookies);
        }
    } else if (!context.credentials_allowed) {
        // A no-credentials request carries none, on any hop of the chain (9.2.1).
        options.headers.remove("Cookie");
    }

    // CORS, recomputed per hop for the same reason the Cookie header is (story
    // 9.2.3). A check applied only to the first request is not a check: a
    // same-origin URL that 302s to another origin would sail through, which is
    // the classic way a strict CORS implementation turns out not to be strict.
    if (chain.cors.enabled()) {
        if (!Core::Cors::is_same_origin(url, chain.cors.document_url)) {
            // Once cross-origin, always: `active` is never cleared, so a chain
            // that wanders off-origin and returns cannot launder its way back
            // into same-origin treatment.
            chain.cors.active = true;
        }
        if (chain.cors.active) {
            options.headers.set("Origin", chain.cors.effective_origin());
        }
    }

    // Recomputed per hop like the Cookie header: the referrer policy depends on
    // this hop's target, so a chain crossing origins re-derives (or drops) the
    // Referer each hop rather than forwarding the first hop's value.
    if (auto referer = Core::compute_referrer_header(chain.referrer_source, url)) {
        options.headers.set("Referer", *referer);
    } else {
        options.headers.remove("Referer");
    }

    // Origin accompanies every non-GET request (Fetch spec). It is the
    // initiating document's origin, not reduced or downgraded, and is what a
    // server's CSRF check reads to confirm the POST came from its own page — the
    // one header a browser adds to a same-origin form POST that a top-level GET
    // navigation lacks. Without it HN answers /comment with 429 "Sorry.".
    if (chain.method != "GET" && chain.method != "HEAD") {
        if (auto origin = Core::compute_origin_header(chain.referrer_source)) {
            options.headers.set("Origin", *origin);
        } else {
            options.headers.remove("Origin");
        }
    }

    // Browser identity (User-Agent + Sec-CH-UA), chosen per target origin and
    // recomputed per hop so a chain crossing origins presents each site the
    // identity selected for it. A null store (tests) means Transparent, and the
    // engine owning this is why CurlNetwork no longer hard-codes a User-Agent.
    if (auto target = Core::Origin::parse(url)) {
        const auto mode = identity_store_ ? identity_store_->mode_for(*target) : Core::IdentityMode::Transparent;
        const bool secure = target->scheme() == "https";
        for (const auto& header : Core::identity_headers(mode, secure)) {
            options.headers.set(header.name, header.value);
        }
    }
    chain.visited.push_back(url);

    // A form POST's outgoing identity/CSRF headers, at DEBUG — invaluable when a
    // write is rejected (e.g. HN's 429), but off in a normal release build.
    if (chain.method != "GET" && chain.method != "HEAD") {
        HB_LOG_DEBUG("[network] " << chain.method << " " << url << " Referer=" << options.headers.get("Referer")
                                  << " Origin=" << options.headers.get("Origin")
                                  << " UA=" << options.headers.get("User-Agent")
                                  << " Cookie=" << (options.headers.get("Cookie").empty() ? "no" : "yes"));
    }

    // --- HTTP cache lookup (story 9.3.1) -------------------------------------
    //
    // Placed HERE, after the Cookie/Origin/Referer/identity headers have been
    // computed and before the transport is called. Those are request headers a
    // response may `Vary` on, so a lookup made before they exist would key the
    // entry on a request that was never sent — which is precisely the bug 9.3.2
    // is about, arrived at by accident.
    //
    // POSTs neither read nor write the cache: the key is method + URL, and a
    // POST's meaning lives in a body the key does not carry.
    std::optional<Core::HttpCache::Lookup> serve_from_cache;
    bool revalidating = false;
    if (http_cache_ && chain.method == "GET" && !request_body && chain.cache_policy != CachePolicy::Bypass) {
        auto hit = http_cache_->lookup("GET", url, options.headers, Core::CacheClock::now());
        // A reload must confirm even a fresh entry. Without this, F5 on a page
        // with `max-age=3600` would show the same bytes for an hour and the user
        // would have no way to refresh it — the cache would have broken the one
        // control they have over it.
        const bool must_ask = chain.cache_policy == CachePolicy::Revalidate;
        if (hit.outcome == Core::HttpCache::Outcome::Fresh && !must_ask) {
            serve_from_cache = std::move(hit);
        } else if (hit.outcome != Core::HttpCache::Outcome::Miss || (must_ask && !hit.etag.empty())) {
            // Stale, or fresh but the user asked. Either way the server is asked
            // whether what we hold is still good, which a 304 answers in a few
            // hundred bytes instead of resending the whole body. Against a CDN
            // this is the common case, not the exception.
            if (!hit.etag.empty()) {
                options.headers.set("If-None-Match", hit.etag);
                revalidating = true;
            } else if (!hit.last_modified.empty()) {
                options.headers.set("If-Modified-Since", hit.last_modified);
                revalidating = true;
            }
        }
    }

    auto handle_response = [this, &network, url, options, callback, context, request_body, revalidating, chain](
                               NetworkResponse response, bool from_cache) mutable {
        // A cached response never re-plants cookies: replaying a `Set-Cookie`
        // from memory would resurrect one the user had deleted. (The cache
        // refuses to store such responses at all, so this is belt and braces.)
        if (cookie_jar_ && context.credentials_allowed && !from_cache && !response.headers.empty()) {
            // Attribute Set-Cookie to this hop's URL. Cookies set mid-chain are
            // therefore stored before the next hop's header is computed, which is
            // what makes a login 302 land authenticated.
            std::lock_guard<std::mutex> lg(cookie_mutex_);
            cookie_jar_->store_from_response(url, response.headers, Core::CookieClock::now());
        }

        // The CORS verdict for THIS hop, before anything else looks at the
        // response — including the redirect machinery. A hop the page may not
        // read is a hop the chain must not continue from either.
        if (chain.cors.active) {
            const auto verdict =
                Core::Cors::check_response(response.headers, chain.cors.effective_origin(), chain.cors.credentials);
            if (verdict != Core::Cors::Decision::Allowed) {
                HB_LOG_WARN("[cors] blocked hop " << chain.hops << " " << url << " for "
                                                  << chain.cors.effective_origin() << ": "
                                                  << Core::Cors::describe(verdict));
                NetworkResponse blocked;
                blocked.url = chain.visited.front();
                blocked.effective_url = url;
                blocked.error = NetworkError::CorsBlocked;
                if (callback) callback(std::move(blocked));
                return;
            }
        }

        // A conditional request came back 304: what we already hold is current.
        // The stored response is swapped in HERE — after the CORS verdict, which
        // must judge the 304 the server actually sent, and before the redirect
        // decision and the exposure filter, so everything below sees exactly what
        // a full response would have looked like.
        if (revalidating && response.status == 304 && response.error == NetworkError::None && http_cache_) {
            auto stored = http_cache_->refresh_from_not_modified("GET", url, options.headers, response.headers,
                                                                 Core::CacheClock::now());
            if (stored) {
                response.status = stored->status;
                response.headers = std::move(stored->headers);
                response.body = std::move(stored->body);
                response.headers.set("Age", std::to_string(stored->age.count()));
                from_cache = true;
            } else if (!chain.revalidation_retried) {
                // The entry was evicted between asking and being answered. A 304
                // carries no body, so there is nothing to hand the page: ask
                // again unconditionally rather than deliver an empty document.
                HB_LOG_DEBUG("[cache] 304 for an entry that is gone; re-requesting " << url);
                NetworkRequestOptions retry = options;
                retry.headers.remove("If-None-Match");
                retry.headers.remove("If-Modified-Since");
                RedirectChain retry_chain = chain;
                retry_chain.revalidation_retried = true;
                // This hop is about to be recorded again; leaving it would make
                // the loop detector call the retry a redirect cycle.
                retry_chain.visited.pop_back();
                send_request(network, url, std::move(retry), std::move(callback), context, std::move(request_body),
                             std::move(retry_chain));
                return;
            } else {
                // Twice in a row means something is wrong beyond eviction. There
                // is no honest response to deliver, so this is a failed request —
                // CurlError for want of a more specific variant, which is not
                // worth adding for a case that needs a cache race to reach.
                HB_LOG_WARN("[cache] repeated 304 with no stored entry: " << url);
                NetworkResponse failed;
                failed.url = chain.visited.front();
                failed.effective_url = url;
                failed.error = NetworkError::CurlError;
                if (callback) callback(std::move(failed));
                return;
            }
        }

        // Store this hop's response — every hop, not only the last, because a
        // cached 301 saves the whole chain next time (which is why the cache
        // belongs inside the redirect loop rather than around it).
        //
        // Deliberately BEFORE the exposure filter further down, so what lands in
        // the cache is what the server actually said and policy re-applies on
        // every later use. Storing the filtered copy would drop
        // Access-Control-Allow-Origin, and the re-check on a subsequent cache hit
        // would then block a response the server had plainly allowed.
        if (http_cache_ && chain.method == "GET" && !request_body && !from_cache &&
            response.error == NetworkError::None) {
            const auto verdict = http_cache_->store("GET", url, response.status, options.headers, response.headers,
                                                    response.body, Core::CacheClock::now());
            if (verdict != Core::Storability::Storable) {
                HB_LOG_DEBUG("[cache] not stored (" << Core::describe(verdict) << "): " << url);
            }
        }

        auto decision = RedirectPolicy::decide(response.status, response.headers.get("Location"), url, chain.method);
        if (decision && (chain.cors.preflighted || chain.cors.is_preflight)) {
            // A preflight asked about the request it was given, not about
            // wherever the server would like to send it next; and a preflight
            // itself may not be redirected. Following either would let a server
            // move the goalposts after the permission was granted.
            HB_LOG_WARN("[cors] refusing to follow a redirect on a "
                        << (chain.cors.is_preflight ? "preflight" : "preflighted request") << ": " << url);
            NetworkResponse blocked;
            blocked.url = chain.visited.front();
            blocked.effective_url = url;
            blocked.error = NetworkError::CorsBlocked;
            if (callback) callback(std::move(blocked));
            return;
        }
        if (!decision) {
            // Not a redirect (or not a followable one): this is the answer.
            if (response.effective_url.empty()) {
                response.effective_url = url;
            }
            // Fetch never exposes cookie-setting response fields to script,
            // even for a same-origin response. The jar and cache have already
            // consumed the unfiltered headers above.
            response.headers = Core::Cors::filter_forbidden_response_headers(response.headers);
            if (chain.cors.active) {
                // Story 9.2.4: a permitted response still does not expose
                // everything it carries. Filtering happens HERE, after the jar
                // has already taken any Set-Cookie above — the browser stores
                // that cookie, the page just never gets to read it.
                response.headers = Core::Cors::filter_exposed_headers(response.headers, chain.cors.credentials);
            }
            if (callback) callback(std::move(response));
            return;
        }

        const auto fail = [&](NetworkError error, const char* what) {
            HB_LOG_WARN("[network] " << what << " after " << chain.hops << " hops: " << url << " -> " << decision->url);
            NetworkResponse failed;
            failed.url = chain.visited.front();
            failed.effective_url = url;
            failed.status = response.status;
            failed.error = error;
            if (callback) callback(std::move(failed));
        };

        if (chain.hops + 1 > RedirectPolicy::kMaxHops) {
            fail(NetworkError::TooManyRedirects, "redirect hop limit exceeded");
            return;
        }
        if (std::find(chain.visited.begin(), chain.visited.end(), decision->url) != chain.visited.end()) {
            fail(NetworkError::RedirectLoop, "redirect loop detected");
            return;
        }

        // A rewritten-to-GET hop carries no body, so its content type is
        // meaningless; leaving it set would advertise a body we are not sending.
        NetworkRequestOptions next = options;
        // The conditional belonged to THIS hop's cached entry. Carrying it to the
        // next URL asks the wrong question, and could earn a 304 for a resource
        // the cache holds nothing for.
        next.headers.remove("If-None-Match");
        next.headers.remove("If-Modified-Since");
        std::optional<std::string> next_body;
        if (decision->method == chain.method) {
            next_body = request_body;
        } else {
            next.content_type.clear();
        }

        // Recompute the SameSite context for the next hop (story 8.1.3). The hop
        // that issued the redirect is the next one's initiator, so a chain that
        // crosses sites makes every later hop cross-site — otherwise an
        // address-bar navigation to A that bounces to B would hand over B's
        // Strict cookies on A's "no initiator, nothing is cross-site" standing.
        Core::CookieRequestContext next_context = context;
        if (auto parts = Core::parse_absolute_url(url)) {
            next_context.initiator_host = parts->host;
        }
        // Safe-method status follows the rewrite: a 302 that turns POST into GET
        // makes the next hop safe; a 307 that preserves POST does not.
        next_context.safe_method = decision->method == "GET" || decision->method == "HEAD";

        // Crossing an origin boundary mid-chain taints the request: from here on
        // it presents `Origin: null`, so the next server must opt in to an
        // opaque origin rather than to the page that started this. Without it, a
        // server could read the initiator's origin off a hop it was never
        // authorized by.
        if (chain.cors.enabled() && !Core::Cors::is_same_origin(decision->url, url)) {
            chain.cors.tainted = true;
        }

        ++chain.hops;
        const std::string previous_method = chain.method;
        chain.method = decision->method;
        HB_LOG_DEBUG("[network] redirect " << response.status << " " << url << " -> " << decision->url << " ("
                                           << previous_method << " -> " << chain.method << ")");
        send_request(network, decision->url, std::move(next), std::move(callback), next_context, std::move(next_body),
                     std::move(chain));
    };

    // A fresh entry answers without the network being touched at all. It still
    // goes through `handle_response`, which is the point: the CORS verdict, the
    // exposure filter and redirect following all re-apply to a cached answer
    // exactly as they would to a live one. A cache that bypassed them would be a
    // way to launder a response past the checks that admitted it.
    if (serve_from_cache) {
        NetworkResponse cached;
        cached.url = url;
        cached.effective_url = url;
        cached.status = serve_from_cache->status;
        cached.headers = std::move(serve_from_cache->headers);
        cached.body = std::move(serve_from_cache->body);
        // `Age` is how a response says how old it is, and the one standard way a
        // same-origin page can see this did not come from the network. It stays
        // invisible cross-origin — Age is not CORS-safelisted, which the M9 demo
        // page shows against Wikipedia.
        cached.headers.set("Age", std::to_string(serve_from_cache->age.count()));
        HB_LOG_DEBUG("[cache] hit (age " << serve_from_cache->age.count() << "s): " << url);
        handle_response(std::move(cached), /*from_cache*/ true);
        return;
    }

    auto on_response = [handle_response](NetworkResponse response) mutable {
        handle_response(std::move(response), /*from_cache*/ false);
    };
    network.request(url, chain.method, request_body ? std::string_view(*request_body) : std::string_view{},
                    std::move(on_response), options);
}

void ResourceLoader::shutdown() {
    active_nav_.store(UINT64_MAX, std::memory_order_release);

    if (network_) network_->shutdown();
    if (fallback_network_) fallback_network_->shutdown();

    {
        std::lock_guard<std::mutex> lg(pending_mutex_);
        pending_resources_.clear();
    }
}

void ResourceLoader::reset() {
    resource_store_.clear();
    std::lock_guard<std::mutex> lg(pending_mutex_);
    pending_resources_.clear();
}

void ResourceLoader::navigate(std::string_view url) {
    navigate(url, {});
}

void ResourceLoader::navigate(std::string_view url, const DocumentRequest& request) {
    const uint64_t id = ++nav_counter_;
    active_nav_.store(id, std::memory_order_release);
    // What the subresources of this navigation inherit — deliberately NOT always
    // the document's own policy.
    //
    // A normal reload revalidates the document only. Browsers used to re-check
    // every subresource on F5 and moved away from it (Chrome ~2017) because on a
    // page with fifty assets it turned a reload into fifty conditional requests,
    // which made the fastest way to reload a page "navigate to it again". A hard
    // reload is where the user is asking for the expensive, thorough thing, so
    // Bypass does propagate.
    nav_cache_policy_.store(request.cache_policy == CachePolicy::Bypass ? CachePolicy::Bypass : CachePolicy::Default,
                            std::memory_order_release);
    // Armed for this navigation's script phase, and reduced the same way
    // subresources are: a NORMAL reload revalidates the document only, so it
    // must not force every API call the page makes to re-fetch. A HARD reload
    // is the gesture that means "get everything again", and it now reaches the
    // page's own fetches too (T-NET-RELOAD-FETCH-POLICY-1).
    script_fetch_policy_.store(request.cache_policy == CachePolicy::Bypass ? CachePolicy::Bypass : CachePolicy::Default,
                               std::memory_order_release);
    std::string url_copy(url);

    // False just means the URL is already tracked as Loading or Ready — i.e. a
    // reload, or a return visit. The fetch proceeds either way and the entry is
    // overwritten when it lands, so this is routine, not a failure.
    if (!resource_store_.begin_request(url_copy, ResourceType::Document)) {
        HB_LOG_DEBUG("[resource] re-requesting already-tracked document: " << url_copy);
    }

    // Built-in about:bookmarks page (7.6.2): rendered synchronously from the
    // shared bookmark file — no network round-trip.
    if (url_copy == "about:bookmarks") {
        Core::BookmarkStore store;
        enqueue_resource_update(ResourceType::Document, url_copy, store.render_html(), /*success*/ true, url_copy);
        return;
    }

    // Refuse to navigate anywhere but http/https (and the built-in pages handled
    // above). A page linking to `file://localhost/C:/…` must not make the engine
    // read local disk and render it as a document — libcurl would happily serve
    // it. The transport blocks this too; doing it here means the rule holds for
    // every backend and shows up as a failed navigation rather than a silent
    // transport error.
    if (!Core::is_fetchable_web_url(url_copy)) {
        HB_LOG_WARN("[network] refusing navigation to non-web scheme: " << url_copy);
        enqueue_resource_update(ResourceType::Document, url_copy, {}, /*success*/ false, url_copy);
        return;
    }

    if (is_builtin_demo_url(url_copy) && fallback_network_) {
        auto callback = [this, id, url_copy](NetworkResponse response) {
            if (id != active_nav_.load(std::memory_order_acquire)) return;
            bool success = !response.body.empty();
            enqueue_resource_update(ResourceType::Document, url_copy, std::move(response.body), success,
                                    std::move(response.effective_url), response.error);
        };
        NetworkRequestOptions options{};
        options.content_type = request.content_type;
        const bool is_post = request.method == DocumentRequest::Method::Post;
        RedirectChain chain;
        chain.method = is_post ? "POST" : "GET";
        chain.referrer_source = request.initiator_url;
        chain.cache_policy = request.cache_policy;
        send_request(*fallback_network_, url_copy, options, std::move(callback),
                     document_cookie_context(is_post, request.initiator_host),
                     is_post ? std::optional<std::string>(request.body) : std::nullopt, std::move(chain));
        return;
    }

    if (!network_) {
        HB_LOG_ERROR("[network] no backend available for " << url);
        if (fallback_network_) {
            auto callback = [this, id, url_copy](NetworkResponse response) {
                if (id != active_nav_.load(std::memory_order_acquire)) return;
                bool success = !response.body.empty();
                enqueue_resource_update(ResourceType::Document, url_copy, std::move(response.body), success,
                                        std::move(response.effective_url), response.error);
            };
            NetworkRequestOptions options{};
            options.content_type = request.content_type;
            const bool is_post = request.method == DocumentRequest::Method::Post;
            RedirectChain chain;
            chain.method = is_post ? "POST" : "GET";
            chain.referrer_source = request.initiator_url;
            chain.cache_policy = request.cache_policy;
            send_request(*fallback_network_, url_copy, options, std::move(callback),
                         document_cookie_context(is_post, request.initiator_host),
                         is_post ? std::optional<std::string>(request.body) : std::nullopt, std::move(chain));
        } else {
            enqueue_resource_update(ResourceType::Document, url_copy, {}, false);
        }
        return;
    }

    NetworkRequestOptions options{};
    options.allow_insecure = is_insecure_allowed_for_url(url_copy);
    options.content_type = request.content_type;

    auto callback = [this, id, url_copy, request_method = request.method, post_body = request.body,
                     post_content_type = request.content_type, initiator_host = request.initiator_host,
                     initiator_url = request.initiator_url,
                     cache_policy = request.cache_policy](NetworkResponse response) {
        if (id != active_nav_.load(std::memory_order_acquire)) return;

        if (response.body.empty()) {
            // A redirect-policy failure is a definitive answer about this URL,
            // not a transport hiccup: retrying it against the stub network would
            // mask a loop behind a demo page. Surface it so the tab (and 8.3.2's
            // error page) can report it.
            if (response.error == NetworkError::TooManyRedirects || response.error == NetworkError::RedirectLoop) {
                HB_LOG_WARN("[network] redirect chain failed for " << url_copy << ", showing error page");
                std::string body = NetworkErrorPage::build(url_copy, response.error);
                enqueue_resource_update(ResourceType::Document, url_copy, std::move(body), /*success*/ true,
                                        std::move(response.effective_url), response.error);
                return;
            }
            if (response.error == NetworkError::TlsVerificationFailed) {
                HB_LOG_WARN("[network] TLS verification failed for " << url_copy << ", showing warning page");
                std::string body = ResourceSecurityPolicy::build_tls_error_body(url_copy);
                enqueue_resource_update(ResourceType::Document, url_copy, std::move(body), true,
                                        std::move(response.effective_url), response.error);
            } else {
                // curl reached nothing (DNS/refused/timeout). Try the stub, which
                // serves the built-in demo pages offline; if it too has nothing for
                // this URL, this is a genuine failure — render the 8.3.2 error page
                // instead of a blank document.
                HB_LOG_WARN("[network] curl returned empty for " << url_copy << ", using stub");
                const NetworkError original_error =
                    response.error == NetworkError::None ? NetworkError::CurlError : response.error;
                if (!fallback_network_) {
                    std::string body = NetworkErrorPage::build(url_copy, original_error);
                    enqueue_resource_update(ResourceType::Document, url_copy, std::move(body), /*success*/ true,
                                            std::move(response.effective_url), original_error);
                    return;
                }
                NetworkRequestOptions fallback_options{};
                fallback_options.content_type = post_content_type;
                RedirectChain fallback_chain;
                fallback_chain.method = request_method == DocumentRequest::Method::Post ? "POST" : "GET";
                fallback_chain.referrer_source = initiator_url;
                fallback_chain.cache_policy = cache_policy;
                send_request(
                    *fallback_network_, url_copy, fallback_options,
                    [this, id, url_copy, original_error](NetworkResponse fallback) {
                        if (id != active_nav_.load(std::memory_order_acquire)) return;
                        if (fallback.body.empty()) {
                            std::string body = NetworkErrorPage::build(url_copy, original_error);
                            enqueue_resource_update(ResourceType::Document, url_copy, std::move(body),
                                                    /*success*/ true, std::move(fallback.effective_url),
                                                    original_error);
                            return;
                        }
                        enqueue_resource_update(ResourceType::Document, url_copy, std::move(fallback.body),
                                                /*success*/ true, std::move(fallback.effective_url), fallback.error);
                    },
                    document_cookie_context(request_method == DocumentRequest::Method::Post, initiator_host),
                    request_method == DocumentRequest::Method::Post ? std::optional<std::string>(post_body)
                                                                    : std::nullopt,
                    std::move(fallback_chain));
            }
            return;
        }

        HB_LOG_INFO("[network] fetched " << response.body.size() << " bytes from " << url_copy);
        enqueue_resource_update(ResourceType::Document, url_copy, std::move(response.body), true,
                                std::move(response.effective_url), response.error);
    };

    const bool is_post = request.method == DocumentRequest::Method::Post;
    RedirectChain chain;
    chain.method = is_post ? "POST" : "GET";
    chain.referrer_source = request.initiator_url;
    chain.cache_policy = request.cache_policy;
    send_request(*network_, url_copy, options, std::move(callback),
                 document_cookie_context(is_post, request.initiator_host),
                 is_post ? std::optional<std::string>(request.body) : std::nullopt, std::move(chain));
}

void ResourceLoader::allow_insecure_host(std::string_view host) {
    security_policy_.allow_insecure_host(host);
}

void ResourceLoader::request_stylesheets(const std::vector<std::string>& links, std::string_view base_url) {
    request_resources(links, base_url, ResourceRequestPlanning::request_options_for(ResourceType::Stylesheet));
}

void ResourceLoader::request_images(const std::vector<std::string>& links, std::string_view base_url) {
    request_resources(links, base_url, ResourceRequestPlanning::request_options_for(ResourceType::Image));
}

void ResourceLoader::request_fonts(const std::vector<std::string>& links, std::string_view base_url) {
    request_resources(links, base_url, ResourceRequestPlanning::request_options_for(ResourceType::Font));
}

void ResourceLoader::request_scripts(const std::vector<std::string>& links, std::string_view base_url) {
    request_resources(links, base_url, ResourceRequestPlanning::request_options_for(ResourceType::Script));
}

void ResourceLoader::end_navigation_script_phase() {
    script_fetch_policy_.store(CachePolicy::Default, std::memory_order_release);
}

ResourceLoader::BatchResult ResourceLoader::consume_pending_updates() {
    auto pending = take_pending_resources();

    BatchResult result{};
    result.pending_count = pending.size();
    if (pending.empty()) return result;

    const auto process_start = Core::Clock::now();
    const size_t pending_count = pending.size();
    ResourceUpdateProcessor::ProcessingStats stats{};

    for (auto& update : pending) {
        ResourceUpdateProcessor::process_update(update, resource_store_, image_decoder_.get(), result, stats);
    }

    const auto process_end = Core::Clock::now();
    std::string ready_summary;
    for (size_t i = 0; i < kResourceTypeCount; ++i) {
        if (!stats.ready[i]) continue;
        if (!ready_summary.empty()) ready_summary += ',';
        ready_summary += ResourceRequestPlanning::request_options_for(static_cast<ResourceType>(i)).type_label;
    }
    HB_LOG_INFO("[perf] resource batch ms=" << Core::duration_ms(process_start, process_end)
                                            << " pending=" << pending_count << " ready=[" << ready_summary
                                            << "] decode_ms=" << stats.image_decode_ms
                                            << " decoded_images=" << stats.image_decode_count);

    // What the filter did, reported by the ENGINE rather than by the extension
    // (story 9.4.2). The measurement must not come from the thing being
    // measured — an extension that reports its own effectiveness is marketing,
    // not telemetry. Logged only when something was actually blocked, so a
    // browser with no blocker installed gains no noise.
    if (request_filter_) {
        const auto blocked = request_filter_->blocked_count();
        if (blocked > 0) {
            HB_LOG_INFO("[filter] blocked requests this session: " << blocked);
        }
    }

    result.ready = stats.ready;
    return result;
}

std::optional<ResourceView> ResourceLoader::view(std::string_view url, ResourceType type) const {
    return resource_store_.view(url, type);
}

const ResourceEntry* ResourceLoader::find(std::string_view url, ResourceType type) const {
    return resource_store_.find(url, type);
}

void ResourceLoader::request_resources(const std::vector<std::string>& links, std::string_view base_url,
                                       const ResourceRequestPlanning::ResourceRequestOptions& options) {
    if (links.empty()) return;

    const uint64_t nav_id = active_nav_.load(std::memory_order_acquire);
    const std::string type_label(options.type_label);

    for (const auto& raw_url : links) {
        auto resolved = ResourceRequestPlanning::resolve_request_url(base_url, raw_url);
        const std::string& url = resolved.key;
        if (url.empty()) continue;

        if (!resource_store_.begin_request(url, options.type)) {
            if (options.log_duplicates) {
                HB_LOG_DEBUG("[resource] " << options.type_label << " already requested: " << url);
            }
            continue;
        }

        HB_LOG_DEBUG("[resource] " << options.type_label << " link: " << options.attr_label << "=" << raw_url
                                   << " base=" << base_url << " resolved=" << url);

        // A `data:` URL carries its own bytes, so it is answered here — before
        // the asset provider is probed and before any transport is chosen
        // (T-NET-DATA-URL-1). It used to fall through to curl, which failed it as
        // a network error: every inline SVG icon cost a request AND rendered
        // nothing. Deliberately ahead of the non-absolute-URL warning below too,
        // since a data URL has no `://` and is not a resolution mistake.
        if (Core::Utils::is_data_url(url)) {
            auto parsed = Core::Utils::parse_data_url(url);
            if (!parsed) {
                HB_LOG_WARN("[resource] malformed data: url for " << options.type_label);
                resource_store_.mark_failed(url, options.type);
                continue;
            }
            HB_LOG_DEBUG("[resource] " << options.type_label << " decoded data: url mime=" << parsed->mime_type
                                       << " bytes=" << parsed->data.size());
            // Same delivery path a bundled asset takes: both are local bytes
            // available immediately, so they must not take two different routes
            // into the store.
            if (options.mark_ready_on_asset) {
                resource_store_.mark_ready(url, options.type, std::move(parsed->data));
            } else {
                enqueue_resource_update(options.type, url, std::move(parsed->data), true);
            }
            continue;
        }

        if (url.find("://") == std::string::npos) {
            HB_LOG_WARN("[resource] " << options.type_label << " resolved to non-absolute url: " << url
                                      << " base=" << base_url);
        }

        if (resource_provider_) {
            // Only probe the local asset provider with paths that can plausibly be
            // asset-relative; root-relative ("/x"), protocol-relative ("//host/x"),
            // UNC ("\\host\x"), and absolute-URL links belong to the document's
            // origin, not our bundled assets, and must never reach the filesystem
            // (T-SEC-URL-1). The same guard applies to the resolved URL, since a
            // protocol-relative link survives resolution unchanged when the base
            // does not parse as absolute.
            auto is_asset_candidate = [](std::string_view candidate) {
                return !candidate.empty() && candidate.front() != '/' && candidate.front() != '\\' &&
                       candidate.find("://") == std::string_view::npos;
            };
            const bool raw_is_asset_candidate = is_asset_candidate(raw_url);
            const bool resolved_is_asset_candidate =
                is_asset_candidate(resolved.resolved) && resolved.resolved != raw_url;
            std::optional<std::string> data;
            if (options.use_binary) {
                if (raw_is_asset_candidate) {
                    data = resource_provider_->load_bytes(raw_url);
                }
                if (!data && resolved_is_asset_candidate) {
                    data = resource_provider_->load_bytes(resolved.resolved);
                }
            } else {
                if (raw_is_asset_candidate) {
                    data = resource_provider_->load_text(raw_url);
                }
                if (!data && resolved_is_asset_candidate) {
                    data = resource_provider_->load_text(resolved.resolved);
                }
            }
            if (data) {
                if (options.log_asset_load) {
                    HB_LOG_DEBUG("[resource] " << options.type_label << " loaded from assets: " << url
                                               << " bytes=" << data->size());
                }
                if (options.mark_ready_on_asset) {
                    resource_store_.mark_ready(url, options.type, std::move(*data));
                } else {
                    enqueue_resource_update(options.type, url, std::move(*data), true);
                }
                continue;
            }
        }

        INetwork* fetcher = network_.get();
        // Route the built-in demo site to the stub, the same way `navigate` and
        // `fetch_for_script` do. Without this a demo page's absolute-path
        // subresource went to the real network and failed DNS while the page
        // around it loaded fine — the identical split that made a fetch to
        // example.dev unresolvable, in the one request path that had not been
        // given the rule. `allow_fallback_network` alone did not cover it: it only
        // applies when there is no primary transport at all, which in the app
        // there always is.
        if (is_builtin_demo_url(url) && fallback_network_) {
            fetcher = fallback_network_.get();
        }
        if (!fetcher && options.allow_fallback_network) {
            fetcher = fallback_network_.get();
        }
        if (!fetcher) {
            HB_LOG_WARN("[resource] no network for " << options.type_label << ": " << url);
            resource_store_.mark_failed(url, options.type);
            continue;
        }

        HB_LOG_DEBUG("[resource] fetching " << options.type_label << ": " << url);
        NetworkRequestOptions request_options{};
        request_options.allow_insecure = security_policy_.is_insecure_allowed_for_url(url);
        RedirectChain chain;
        chain.referrer_source = std::string(base_url);
        // Subresources inherit the navigation's cache policy: a reload that
        // revalidated the document but served its stylesheet from cache would
        // refresh the words and not the layout.
        chain.cache_policy = nav_cache_policy_.load(std::memory_order_acquire);
        chain.destination = filter_destination_for(options.type);
        send_request(
            *fetcher, url, request_options,
            [this, nav_id, url, type = options.type, type_label](NetworkResponse response) {
                if (nav_id != active_nav_.load(std::memory_order_acquire)) return;
                bool success = !response.body.empty();
                // A filtered request produces no body, but it is not a failure
                // and must not be logged as one (story 9.4.1). `[filter]
                // blocked` has already reported it, with the rule that decided
                // — so this would be a second, wronger line about the same
                // event. An ad blocker that fills the log with warnings about
                // the requests it was installed to prevent is how real warnings
                // stop being read.
                if (!success && response.error != NetworkError::BlockedByFilter) {
                    HB_LOG_WARN("[resource] " << type_label << " fetch failed: " << url);
                }
                enqueue_resource_update(type, url, std::move(response.body), success, {}, response.error);
            },
            subresource_cookie_context(base_url), std::nullopt, std::move(chain));
    }
}

void ResourceLoader::enqueue_resource_update(ResourceType type, std::string url, std::string body, bool success,
                                             std::string effective_url, NetworkError error) {
    std::lock_guard<std::mutex> lg(pending_mutex_);
    pending_resources_.push_back({type, std::move(url), std::move(effective_url), std::move(body), success, error});
}

std::vector<ResourceLoader::PendingResourceUpdate> ResourceLoader::take_pending_resources() {
    std::vector<PendingResourceUpdate> pending;
    std::lock_guard<std::mutex> lg(pending_mutex_);
    pending.swap(pending_resources_);
    return pending;
}

bool ResourceLoader::is_insecure_allowed_for_url(std::string_view url) const {
    return security_policy_.is_insecure_allowed_for_url(url);
}

}  // namespace Hummingbird::Engine
