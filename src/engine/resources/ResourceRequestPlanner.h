#pragma once

#include <string>
#include <string_view>

#include "engine/resources/ResourceStore.h"

namespace Hummingbird::Engine::ResourceRequestPlanning {

struct ResourceRequestOptions {
    ResourceType type = ResourceType::Document;
    std::string_view type_label;
    std::string_view attr_label;
    bool allow_fallback_network = false;
    bool log_duplicates = false;
    bool log_asset_load = false;
    bool mark_ready_on_asset = false;
    bool use_binary = false;
};

struct ResolvedRequestUrl {
    std::string key;
    std::string resolved;
};

ResourceRequestOptions stylesheet_request_options();
ResourceRequestOptions image_request_options();
ResourceRequestOptions font_request_options();
ResolvedRequestUrl resolve_request_url(std::string_view base_url, std::string_view raw_url);

}  // namespace Hummingbird::Engine::ResourceRequestPlanning
