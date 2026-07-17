#include "engine/resources/ResourceRequestPlanner.h"

#include <iterator>

#include "engine/resources/ResourceUrl.h"

namespace Hummingbird::Engine::ResourceRequestPlanning {

namespace {

// Document is fetched via ResourceLoader::navigate, never request_resources,
// so only its label and decode mode matter. Fonts are opaque binary blobs
// (TTF/OTF) stored raw for the font resolver's on-disk cache — no decode.
constexpr ResourceRequestOptions kResourceTypeTable[] = {
    {
        .type = ResourceType::Document,
        .type_label = "document",
        .attr_label = "href",
        .allow_fallback_network = true,
        .log_duplicates = false,
        .log_asset_load = false,
        .mark_ready_on_asset = false,
        .use_binary = false,
        .decode = ResourceDecode::None,
    },
    {
        .type = ResourceType::Stylesheet,
        .type_label = "stylesheet",
        .attr_label = "href",
        .allow_fallback_network = false,
        .log_duplicates = true,
        .log_asset_load = true,
        .mark_ready_on_asset = true,
        .use_binary = false,
        .decode = ResourceDecode::None,
    },
    {
        .type = ResourceType::Image,
        .type_label = "image",
        .attr_label = "src",
        .allow_fallback_network = true,
        .log_duplicates = false,
        .log_asset_load = false,
        .mark_ready_on_asset = false,
        .use_binary = true,
        .decode = ResourceDecode::Image,
    },
    {
        .type = ResourceType::Font,
        .type_label = "font",
        .attr_label = "src",
        .allow_fallback_network = true,
        .log_duplicates = false,
        .log_asset_load = true,
        .mark_ready_on_asset = true,
        .use_binary = true,
        .decode = ResourceDecode::None,
    },
};

static_assert(std::size(kResourceTypeTable) == kResourceTypeCount, "every ResourceType needs a descriptor table entry");
static_assert(
    [] {
        for (size_t i = 0; i < std::size(kResourceTypeTable); ++i) {
            if (kResourceTypeTable[i].type != static_cast<ResourceType>(i)) return false;
        }
        return true;
    }(),
    "descriptor table entries must be ordered by ResourceType value");

}  // namespace

const ResourceRequestOptions& request_options_for(ResourceType type) {
    return kResourceTypeTable[static_cast<size_t>(type)];
}

ResolvedRequestUrl resolve_request_url(std::string_view base_url, std::string_view raw_url) {
    auto resolved = resolve_resource_url(base_url, raw_url);
    return {resolved.key, resolved.resolved};
}

}  // namespace Hummingbird::Engine::ResourceRequestPlanning
