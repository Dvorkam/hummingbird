#include "engine/resources/ResourceLoader.h"

#include <functional>
#include <ostream>
#include <utility>

#include "core/utils/Log.h"
#include "core/utils/Timing.h"
#include "engine/resources/ResourceRequestPlanner.h"
#include "engine/resources/ResourceUpdateProcessor.h"

namespace Hummingbird::Engine {

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
    navigate(url, {});
}

void ResourceLoader::navigate(std::string_view url, const DocumentRequest& request) {
    const uint64_t id = ++nav_counter_;
    active_nav_.store(id, std::memory_order_release);
    std::string url_copy(url);

    if (!resource_store_.begin_request(url_copy, ResourceType::Document)) {
        HB_LOG_WARN("[resource] failed to register document request: " << url_copy);
    }

    // Built-in demo URL: keep startup deterministic and avoid network timeouts.
    if ((url_copy == "http://example.dev" || url_copy == "https://example.dev") && fallback_network_) {
        auto callback = [this, id, url_copy](NetworkResponse response) {
            if (id != active_nav_.load(std::memory_order_acquire)) return;
            bool success = !response.body.empty();
            enqueue_resource_update(ResourceType::Document, url_copy, std::move(response.body), success,
                                    std::move(response.effective_url), response.error);
        };
        if (request.method == DocumentRequest::Method::Post) {
            NetworkRequestOptions post_options{};
            post_options.content_type = request.content_type;
            fallback_network_->post(url_copy, request.body, std::move(callback), post_options);
        } else {
            fallback_network_->get(url_copy, std::move(callback));
        }
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
            if (request.method == DocumentRequest::Method::Post) {
                NetworkRequestOptions post_options{};
                post_options.content_type = request.content_type;
                fallback_network_->post(url_copy, request.body, std::move(callback), post_options);
            } else {
                fallback_network_->get(url_copy, std::move(callback));
            }
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
                if (request_method == DocumentRequest::Method::Post) {
                    NetworkRequestOptions post_options{};
                    post_options.content_type = post_content_type;
                    fallback_network_->post(
                        url_copy, post_body,
                        [this, id, url_copy](NetworkResponse fallback) {
                            if (id != active_nav_.load(std::memory_order_acquire)) return;
                            bool success = !fallback.body.empty();
                            enqueue_resource_update(ResourceType::Document, url_copy, std::move(fallback.body), success,
                                                    std::move(fallback.effective_url), fallback.error);
                        },
                        post_options);
                } else {
                    fallback_network_->get(url_copy, [this, id, url_copy](NetworkResponse fallback) {
                        if (id != active_nav_.load(std::memory_order_acquire)) return;
                        bool success = !fallback.body.empty();
                        enqueue_resource_update(ResourceType::Document, url_copy, std::move(fallback.body), success,
                                                std::move(fallback.effective_url), fallback.error);
                    });
                }
            }
            return;
        }

        HB_LOG_INFO("[network] fetched " << response.body.size() << " bytes from " << url_copy);
        enqueue_resource_update(ResourceType::Document, url_copy, std::move(response.body), true,
                                std::move(response.effective_url), response.error);
    };

    if (request.method == DocumentRequest::Method::Post) {
        network_->post(url_copy, request.body, std::move(callback), options);
    } else {
        network_->get(url_copy, std::move(callback), options);
    }
}

void ResourceLoader::allow_insecure_host(std::string_view host) {
    security_policy_.allow_insecure_host(host);
}

void ResourceLoader::request_stylesheets(const std::vector<std::string>& links, std::string_view base_url) {
    request_resources(links, base_url, ResourceRequestPlanning::stylesheet_request_options());
}

void ResourceLoader::request_images(const std::vector<std::string>& links, std::string_view base_url) {
    request_resources(links, base_url, ResourceRequestPlanning::image_request_options());
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
    HB_LOG_INFO("[perf] resource batch ms="
                << Core::duration_ms(process_start, process_end) << " pending=" << pending_count
                << " doc=" << static_cast<int>(stats.document_ready)
                << " styles=" << static_cast<int>(stats.stylesheet_ready)
                << " images=" << static_cast<int>(stats.image_ready) << " decode_ms=" << stats.image_decode_ms
                << " decoded_images=" << stats.image_decode_count);

    result.document_ready = stats.document_ready;
    result.stylesheet_ready = stats.stylesheet_ready;
    result.image_ready = stats.image_ready;
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
        request_options.allow_insecure = security_policy_.is_insecure_allowed_for_url(url);
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
    return security_policy_.is_insecure_allowed_for_url(url);
}

}  // namespace Hummingbird::Engine
