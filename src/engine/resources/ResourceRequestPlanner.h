#pragma once

#include <string>
#include <string_view>

#include "engine/resources/ResourceStore.h"

namespace Hummingbird::Engine::ResourceRequestPlanning {

// How the update processor turns fetched bytes into a ready resource.
enum class ResourceDecode : uint8_t {
    None,   // store the body as-is (text or opaque binary)
    Image,  // run the image decoder (bitmap or animation) before marking ready
};

struct ResourceRequestOptions {
    ResourceType type = ResourceType::Document;
    std::string_view type_label;
    std::string_view attr_label;
    bool allow_fallback_network = false;
    bool log_duplicates = false;
    bool log_asset_load = false;
    bool mark_ready_on_asset = false;
    bool use_binary = false;
    ResourceDecode decode = ResourceDecode::None;
};

// One descriptor per ResourceType; the loader's request path and the update
// processor's ready path both dispatch off this table, so adding a resource
// type is one enum value plus one table entry (T-RESOURCE-TYPE-TABLE-1) —
// not a hand-mirrored edit across every layer.
const ResourceRequestOptions& request_options_for(ResourceType type);

struct ResolvedRequestUrl {
    std::string key;
    std::string resolved;
};

ResolvedRequestUrl resolve_request_url(std::string_view base_url, std::string_view raw_url);

}  // namespace Hummingbird::Engine::ResourceRequestPlanning
