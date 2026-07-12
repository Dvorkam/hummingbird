#include "engine/resources/ResourceRequestPlanner.h"

#include "engine/resources/ResourceUrl.h"

namespace Hummingbird::Engine::ResourceRequestPlanning {

ResourceRequestOptions stylesheet_request_options() {
    ResourceRequestOptions options{};
    options.type = ResourceType::Stylesheet;
    options.type_label = "stylesheet";
    options.attr_label = "href";
    options.allow_fallback_network = false;
    options.log_duplicates = true;
    options.log_asset_load = true;
    options.mark_ready_on_asset = true;
    options.use_binary = false;
    return options;
}

ResourceRequestOptions image_request_options() {
    ResourceRequestOptions options{};
    options.type = ResourceType::Image;
    options.type_label = "image";
    options.attr_label = "src";
    options.allow_fallback_network = true;
    options.log_duplicates = false;
    options.log_asset_load = false;
    options.mark_ready_on_asset = false;
    options.use_binary = true;
    return options;
}

ResolvedRequestUrl resolve_request_url(std::string_view base_url, std::string_view raw_url) {
    auto resolved = resolve_resource_url(base_url, raw_url);
    return {resolved.key, resolved.resolved};
}

}  // namespace Hummingbird::Engine::ResourceRequestPlanning
