#include "engine/resources/ResourceLoader.h"

#include <functional>
#include <ostream>
#include <utility>

#include "core/BookmarkStore.h"
#include "core/utils/Log.h"
#include "core/utils/Timing.h"
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
}  // namespace

ResourceLoader::ResourceLoader(NetworkPtr network, NetworkPtr fallback_network, ResourceProviderPtr resource_provider,
                               ImageDecoderPtr image_decoder, std::shared_ptr<Core::CookieJar> cookie_jar)
    : network_(std::move(network)),
      fallback_network_(std::move(fallback_network)),
      resource_provider_(std::move(resource_provider)),
      image_decoder_(std::move(image_decoder)),
      cookie_jar_(std::move(cookie_jar)) {
    if (!network_ || !fallback_network_) {
        HB_LOG_ERROR("[network] failed to create network backend(s)");
    }
    if (!resource_provider_) {
        HB_LOG_WARN("[resource] no resource provider available");
    }
    if (!image_decoder_) {
        HB_LOG_WARN("[image] no decoder available");
    }
}

void ResourceLoader::send_request(INetwork& network, const std::string& url, NetworkRequestOptions options,
                                  std::function<void(NetworkResponse)> callback, const std::string* post_body) {
    if (cookie_jar_) {
        std::lock_guard<std::mutex> lg(cookie_mutex_);
        const std::string cookies = cookie_jar_->cookie_header_for(url, Core::CookieClock::now());
        if (!cookies.empty()) {
            options.headers.set("Cookie", cookies);
        }
    }

    auto with_cookie_capture = [this, url, callback = std::move(callback)](NetworkResponse response) mutable {
        if (cookie_jar_ && !response.headers.empty()) {
            // Attribute Set-Cookie to the URL that actually served the response,
            // which is what the redirect chain landed on.
            const std::string& origin = response.effective_url.empty() ? url : response.effective_url;
            std::lock_guard<std::mutex> lg(cookie_mutex_);
            cookie_jar_->store_from_response(origin, response.headers, Core::CookieClock::now());
        }
        if (callback) callback(std::move(response));
    };

    if (post_body) {
        network.post(url, *post_body, std::move(with_cookie_capture), options);
    } else {
        network.get(url, std::move(with_cookie_capture), options);
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

    if (!resource_store_.begin_request(url_copy, ResourceType::Document)) {
        HB_LOG_WARN("[resource] failed to register document request: " << url_copy);
    }

    // Built-in about:bookmarks page (7.6.2): rendered synchronously from the
    // shared bookmark file — no network round-trip.
    if (url_copy == "about:bookmarks") {
        Core::BookmarkStore store;
        enqueue_resource_update(ResourceType::Document, url_copy, store.render_html(), /*success*/ true, url_copy);
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
        send_request(*fallback_network_, url_copy, options, std::move(callback), is_post ? &request.body : nullptr);
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
            send_request(*fallback_network_, url_copy, options, std::move(callback), is_post ? &request.body : nullptr);
        } else {
            enqueue_resource_update(ResourceType::Document, url_copy, {}, false);
        }
        return;
    }

    NetworkRequestOptions options{};
    options.allow_insecure = is_insecure_allowed_for_url(url_copy);
    options.content_type = request.content_type;

    auto callback = [this, id, url_copy, request_method = request.method, post_body = request.body,
                     post_content_type = request.content_type](NetworkResponse response) {
        if (id != active_nav_.load(std::memory_order_acquire)) return;

        if (response.body.empty()) {
            if (response.error == NetworkError::TlsVerificationFailed) {
                HB_LOG_WARN("[network] TLS verification failed for " << url_copy << ", showing warning page");
                std::string body = ResourceSecurityPolicy::build_tls_error_body(url_copy);
                enqueue_resource_update(ResourceType::Document, url_copy, std::move(body), true,
                                        std::move(response.effective_url), response.error);
            } else {
                HB_LOG_WARN("[network] curl returned empty for " << url_copy << ", using stub");
                if (!fallback_network_) return;
                NetworkRequestOptions fallback_options{};
                fallback_options.content_type = post_content_type;
                send_request(
                    *fallback_network_, url_copy, fallback_options,
                    [this, id, url_copy](NetworkResponse fallback) {
                        if (id != active_nav_.load(std::memory_order_acquire)) return;
                        bool success = !fallback.body.empty();
                        enqueue_resource_update(ResourceType::Document, url_copy, std::move(fallback.body), success,
                                                std::move(fallback.effective_url), fallback.error);
                    },
                    request_method == DocumentRequest::Method::Post ? &post_body : nullptr);
            }
            return;
        }

        HB_LOG_INFO("[network] fetched " << response.body.size() << " bytes from " << url_copy);
        enqueue_resource_update(ResourceType::Document, url_copy, std::move(response.body), true,
                                std::move(response.effective_url), response.error);
    };

    const bool is_post = request.method == DocumentRequest::Method::Post;
    send_request(*network_, url_copy, options, std::move(callback), is_post ? &request.body : nullptr);
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
        send_request(*fetcher, url, request_options,
                     [this, nav_id, url, type = options.type, type_label](NetworkResponse response) {
                         if (nav_id != active_nav_.load(std::memory_order_acquire)) return;
                         bool success = !response.body.empty();
                         if (!success) {
                             HB_LOG_WARN("[resource] " << type_label << " fetch failed: " << url);
                         }
                         enqueue_resource_update(type, url, std::move(response.body), success, {}, response.error);
                     });
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
