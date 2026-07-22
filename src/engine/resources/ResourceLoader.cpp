#include "engine/resources/ResourceLoader.h"

#include <algorithm>
#include <functional>
#include <optional>
#include <ostream>
#include <utility>

#include "core/BookmarkStore.h"
#include "core/net/Referrer.h"
#include "core/utils/Log.h"
#include "core/utils/Url.h"
#include "core/utils/Timing.h"
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
    for (std::string_view prefix : {"http://example.dev", "https://example.dev"}) {
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
}  // namespace

ResourceLoader::ResourceLoader(NetworkPtr network, NetworkPtr fallback_network, ResourceProviderPtr resource_provider,
                               ImageDecoderPtr image_decoder, std::shared_ptr<Core::CookieJar> cookie_jar,
                               std::shared_ptr<Core::IdentityPolicyStore> identity_store)
    : network_(std::move(network)),
      fallback_network_(std::move(fallback_network)),
      resource_provider_(std::move(resource_provider)),
      image_decoder_(std::move(image_decoder)),
      cookie_jar_(std::move(cookie_jar)),
      identity_store_(std::move(identity_store)) {
    // Only "nothing can fetch at all" is an error. Having just one backend is a
    // deliberate configuration (tests, headless harnesses, stub-only demo runs),
    // and shouting about it on every construction trained the eye to ignore a
    // line that should mean something.
    if (!network_ && !fallback_network_) {
        HB_LOG_ERROR("[network] no network backend available; requests will fail");
    } else if (!network_ || !fallback_network_) {
        HB_LOG_DEBUG("[network] single backend configured (primary=" << (network_ ? "yes" : "no")
                                                                     << " fallback=" << (fallback_network_ ? "yes" : "no")
                                                                     << ")");
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

void ResourceLoader::send_request(INetwork& network, const std::string& url, NetworkRequestOptions options,
                                  std::function<void(NetworkResponse)> callback,
                                  const Core::CookieRequestContext& context, std::optional<std::string> post_body,
                                  RedirectChain chain) {
    // Recomputed per hop: a chain can cross hosts and paths, so the previous
    // hop's Cookie header must never ride along.
    if (cookie_jar_) {
        std::lock_guard<std::mutex> lg(cookie_mutex_);
        const std::string cookies = cookie_jar_->cookie_header_for(url, Core::CookieClock::now(), context);
        if (cookies.empty()) {
            options.headers.remove("Cookie");
        } else {
            options.headers.set("Cookie", cookies);
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
    if (post_body) {
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
    if (post_body) {
        HB_LOG_DEBUG("[network] POST " << url << " Referer=" << options.headers.get("Referer") << " Origin="
                                       << options.headers.get("Origin") << " UA=" << options.headers.get("User-Agent")
                                       << " Cookie=" << (options.headers.get("Cookie").empty() ? "no" : "yes"));
    }

    auto on_response = [this, &network, url, options, callback, context, post_body,
                        chain](NetworkResponse response) mutable {
        if (cookie_jar_ && !response.headers.empty()) {
            // Attribute Set-Cookie to this hop's URL. Cookies set mid-chain are
            // therefore stored before the next hop's header is computed, which is
            // what makes a login 302 land authenticated.
            std::lock_guard<std::mutex> lg(cookie_mutex_);
            cookie_jar_->store_from_response(url, response.headers, Core::CookieClock::now());
        }

        auto decision = RedirectPolicy::decide(response.status, response.headers.get("Location"), url,
                                               post_body.has_value());
        if (!decision) {
            // Not a redirect (or not a followable one): this is the answer.
            if (response.effective_url.empty()) {
                response.effective_url = url;
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
        std::optional<std::string> next_body;
        if (decision->keep_post) {
            next_body = post_body;
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
        next_context.safe_method = !decision->keep_post;

        ++chain.hops;
        HB_LOG_DEBUG("[network] redirect " << response.status << " " << url << " -> " << decision->url
                                           << (decision->keep_post ? " (POST preserved)" : " (as GET)"));
        send_request(network, decision->url, std::move(next), std::move(callback), next_context, std::move(next_body),
                     std::move(chain));
    };

    if (post_body) {
        network.post(url, *post_body, std::move(on_response), options);
    } else {
        network.get(url, std::move(on_response), options);
    }
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
        chain.referrer_source = request.initiator_url;
        send_request(*fallback_network_, url_copy, options, std::move(callback), document_cookie_context(is_post, request.initiator_host),
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
            chain.referrer_source = request.initiator_url;
            send_request(*fallback_network_, url_copy, options, std::move(callback), document_cookie_context(is_post, request.initiator_host),
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
                     initiator_url = request.initiator_url](NetworkResponse response) {
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
                fallback_chain.referrer_source = initiator_url;
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
                    request_method == DocumentRequest::Method::Post ? std::optional<std::string>(post_body) : std::nullopt,
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
    chain.referrer_source = request.initiator_url;
    send_request(*network_, url_copy, options, std::move(callback), document_cookie_context(is_post, request.initiator_host),
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
        send_request(*fetcher, url, request_options,
                     [this, nav_id, url, type = options.type, type_label](NetworkResponse response) {
                         if (nav_id != active_nav_.load(std::memory_order_acquire)) return;
                         bool success = !response.body.empty();
                         if (!success) {
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
