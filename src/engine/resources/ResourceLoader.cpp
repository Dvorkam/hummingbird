#include "engine/resources/ResourceLoader.h"

#include <algorithm>
#include <cctype>
#include <functional>
#include <ostream>
#include <utility>

#include "core/utils/Log.h"
#include "core/utils/Timing.h"
#include "core/utils/Url.h"
#include "engine/ResourceUrl.h"

namespace Hummingbird::Engine {

namespace {
std::string build_tls_error_body(const std::string& url) {
    std::string body;
    body.reserve(512 + url.size());
    body += R"HTML(<!doctype html>
<html>
  <head>
    <meta charset="utf-8">
    <title>TLS Verification Failed</title>
    <style>
      body { margin: 24px; font-family: sans-serif; color: #222; }
      h1 { margin-bottom: 12px; }
      code { background: #eee; padding: 2px 4px; }
      .hint { margin-top: 16px; }
    </style>
  </head>
  <body>
    <h1>Secure connection failed</h1>
    <p>Hummingbird could not verify the TLS certificate for:</p>
    <p><code>)HTML";
    body += url;
    body += R"HTML(</code></p>
    <p class="hint">If you trust this site, click the insecure icon in the URL bar to proceed once.</p>
  </body>
</html>
)HTML";
    return body;
}
}  // namespace

ResourceLoader::ResourceLoader(NetworkPtr network, NetworkPtr fallback_network, ResourceProviderPtr resource_provider,
                               ImageDecoderPtr image_decoder)
    : network_(std::move(network)),
      fallback_network_(std::move(fallback_network)),
      resource_provider_(std::move(resource_provider)),
      image_decoder_(std::move(image_decoder)) {
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
    const uint64_t id = ++nav_counter_;
    active_nav_.store(id, std::memory_order_release);
    std::string url_copy(url);

    if (!resource_store_.begin_request(url_copy, ResourceType::Document)) {
        HB_LOG_WARN("[resource] failed to register document request: " << url_copy);
    }

    // Built-in demo URL: keep startup deterministic and avoid network timeouts.
    if ((url_copy == "http://example.dev" || url_copy == "https://example.dev") && fallback_network_) {
        fallback_network_->get(url_copy, [this, id, url_copy](NetworkResponse response) {
            if (id != active_nav_.load(std::memory_order_acquire)) return;
            bool success = !response.body.empty();
            enqueue_resource_update(ResourceType::Document, url_copy, std::move(response.body), success,
                                    std::move(response.effective_url), response.error);
        });
        return;
    }

    if (!network_) {
        HB_LOG_ERROR("[network] no backend available for " << url);
        if (fallback_network_) {
            fallback_network_->get(url_copy, [this, id, url_copy](NetworkResponse response) {
                if (id != active_nav_.load(std::memory_order_acquire)) return;
                bool success = !response.body.empty();
                enqueue_resource_update(ResourceType::Document, url_copy, std::move(response.body), success,
                                        std::move(response.effective_url), response.error);
            });
        } else {
            enqueue_resource_update(ResourceType::Document, url_copy, {}, false);
        }
        return;
    }

    NetworkRequestOptions options{};
    options.allow_insecure = is_insecure_allowed_for_url(url_copy);
    network_->get(
        url_copy,
        [this, id, url_copy](NetworkResponse response) {
            if (id != active_nav_.load(std::memory_order_acquire)) return;

            if (response.body.empty()) {
                if (response.error == NetworkError::TlsVerificationFailed) {
                    HB_LOG_WARN("[network] TLS verification failed for " << url_copy << ", showing warning page");
                    std::string body = build_tls_error_body(url_copy);
                    enqueue_resource_update(ResourceType::Document, url_copy, std::move(body), true,
                                            std::move(response.effective_url), response.error);
                } else {
                    HB_LOG_WARN("[network] curl returned empty for " << url_copy << ", using stub");
                    if (!fallback_network_) return;
                    fallback_network_->get(url_copy, [this, id, url_copy](NetworkResponse fallback) {
                        if (id != active_nav_.load(std::memory_order_acquire)) return;
                        bool success = !fallback.body.empty();
                        enqueue_resource_update(ResourceType::Document, url_copy, std::move(fallback.body), success,
                                                std::move(fallback.effective_url), fallback.error);
                    });
                }
                return;
            }

            HB_LOG_INFO("[network] fetched " << response.body.size() << " bytes from " << url_copy);
            enqueue_resource_update(ResourceType::Document, url_copy, std::move(response.body), true,
                                    std::move(response.effective_url), response.error);
        },
        options);
}

void ResourceLoader::allow_insecure_host(std::string_view host) {
    const std::string normalized = normalize_host(host);
    if (!normalized.empty()) {
        insecure_hosts_.insert(normalized);
        HB_LOG_WARN("[network] allowing insecure TLS for host: " << normalized);
    }
}

void ResourceLoader::request_stylesheets(const std::vector<std::string>& links, std::string_view base_url) {
    ResourceRequestOptions options{};
    options.type = ResourceType::Stylesheet;
    options.type_label = "stylesheet";
    options.attr_label = "href";
    options.allow_fallback_network = false;
    options.log_duplicates = true;
    options.log_asset_load = true;
    options.mark_ready_on_asset = true;
    options.use_binary = false;

    request_resources(links, base_url, options);
}

void ResourceLoader::request_images(const std::vector<std::string>& links, std::string_view base_url) {
    ResourceRequestOptions options{};
    options.type = ResourceType::Image;
    options.type_label = "image";
    options.attr_label = "src";
    options.allow_fallback_network = true;
    options.log_duplicates = false;
    options.log_asset_load = false;
    options.mark_ready_on_asset = false;
    options.use_binary = true;

    request_resources(links, base_url, options);
}

ResourceLoader::BatchResult ResourceLoader::consume_pending_updates() {
    auto pending = take_pending_resources();

    BatchResult result{};
    result.pending_count = pending.size();
    if (pending.empty()) return result;

    const auto process_start = Core::Clock::now();
    const size_t pending_count = pending.size();
    bool document_ready = false;
    bool stylesheet_ready = false;
    bool image_ready = false;
    size_t image_decode_count = 0;
    double image_decode_ms = 0.0;

    for (auto& update : pending) {
        if (update.success) {
            if (update.type == ResourceType::Document) {
                handle_document_update(update, result, document_ready);
            } else if (update.type == ResourceType::Stylesheet) {
                handle_stylesheet_update(update, stylesheet_ready);
            } else if (update.type == ResourceType::Image) {
                handle_image_update(update, image_ready, image_decode_count, image_decode_ms);
            }
        } else {
            handle_failed_update(update);
        }
    }

    const auto process_end = Core::Clock::now();
    HB_LOG_INFO("[perf] resource batch ms="
                << Core::duration_ms(process_start, process_end) << " pending=" << pending_count
                << " doc=" << static_cast<int>(document_ready) << " styles=" << static_cast<int>(stylesheet_ready)
                << " images=" << static_cast<int>(image_ready) << " decode_ms=" << image_decode_ms
                << " decoded_images=" << image_decode_count);

    result.document_ready = document_ready;
    result.stylesheet_ready = stylesheet_ready;
    result.image_ready = image_ready;
    return result;
}

void ResourceLoader::handle_document_update(PendingResourceUpdate& update, BatchResult& result, bool& document_ready) {
    resource_store_.mark_ready(update.url, update.type, std::move(update.body));
    document_ready = true;
    result.document_url = update.url;
    result.effective_url = update.effective_url;
    result.document_error = update.error;
}

void ResourceLoader::handle_stylesheet_update(PendingResourceUpdate& update, bool& stylesheet_ready) {
    resource_store_.mark_ready(update.url, update.type, std::move(update.body));
    stylesheet_ready = true;
}

void ResourceLoader::handle_image_update(PendingResourceUpdate& update, bool& image_ready, size_t& image_decode_count,
                                         double& image_decode_ms) {
    if (!image_decoder_) {
        HB_LOG_WARN("[image] decode skipped (no decoder): " << update.url);
        resource_store_.mark_failed(update.url, update.type);
        return;
    }
    const auto decode_start = Core::Clock::now();
    auto decoded = image_decoder_->decode(update.body);
    const auto decode_end = Core::Clock::now();
    image_decode_ms += Core::duration_ms(decode_start, decode_end);
    ++image_decode_count;
    if (!decoded) {
        HB_LOG_WARN("[image] decode failed: " << update.url);
        resource_store_.mark_failed(update.url, update.type);
        return;
    }
    resource_store_.mark_ready(update.url, update.type, std::move(update.body));
    resource_store_.set_image(update.url, update.type, std::move(*decoded));
    image_ready = true;
}

void ResourceLoader::handle_failed_update(const PendingResourceUpdate& update) {
    resource_store_.mark_failed(update.url, update.type);
    if (update.type == ResourceType::Document) {
        HB_LOG_WARN("[resource] document failed to load: " << update.url);
    }
}

std::optional<ResourceView> ResourceLoader::view(std::string_view url, ResourceType type) const {
    return resource_store_.view(url, type);
}

const ResourceEntry* ResourceLoader::find(std::string_view url, ResourceType type) const {
    return resource_store_.find(url, type);
}

void ResourceLoader::request_resources(const std::vector<std::string>& links, std::string_view base_url,
                                       const ResourceRequestOptions& options) {
    if (links.empty()) return;

    const uint64_t nav_id = active_nav_.load(std::memory_order_acquire);
    const std::string type_label(options.type_label);

    for (const auto& raw_url : links) {
        auto resolved = resolve_resource_url(base_url, raw_url);
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
            std::optional<std::string> data;
            if (options.use_binary) {
                data = resource_provider_->load_bytes(raw_url);
                if (!data && !resolved.resolved.empty() && resolved.resolved != raw_url) {
                    data = resource_provider_->load_bytes(resolved.resolved);
                }
            } else {
                data = resource_provider_->load_text(raw_url);
                if (!data && !resolved.resolved.empty() && resolved.resolved != raw_url) {
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
        request_options.allow_insecure = is_insecure_allowed_for_url(url);
        fetcher->get(
            url,
            [this, nav_id, url, type = options.type, type_label](NetworkResponse response) {
                if (nav_id != active_nav_.load(std::memory_order_acquire)) return;
                bool success = !response.body.empty();
                if (!success) {
                    HB_LOG_WARN("[resource] " << type_label << " fetch failed: " << url);
                }
                enqueue_resource_update(type, url, std::move(response.body), success, {}, response.error);
            },
            request_options);
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
    auto parsed = Core::parse_absolute_url(url);
    if (!parsed) return false;
    if (parsed->scheme != "https") return false;
    const std::string host = normalize_host(parsed->host);
    if (host.empty()) return false;
    return insecure_hosts_.find(host) != insecure_hosts_.end();
}

std::string ResourceLoader::normalize_host(std::string_view host) {
    std::string normalized(host);
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return normalized;
}

}  // namespace Hummingbird::Engine
