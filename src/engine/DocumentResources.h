#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace Hummingbird::Layout {
class RenderObject;
}

namespace Hummingbird {
class IResourceProvider;
}

namespace Hummingbird::Engine {

class ResourceStore;

class DocumentResources {
public:
    DocumentResources(ResourceStore* resource_store, IResourceProvider* resource_provider)
        : resource_store_(resource_store), resource_provider_(resource_provider) {}

    std::string build_css_source(std::string_view base_url, const std::vector<std::string>& style_blocks,
                                 const std::vector<std::string>& stylesheet_links) const;
    bool update_image_resources(Layout::RenderObject* render_tree, std::string_view base_url) const;

private:
    ResourceStore* resource_store_ = nullptr;
    IResourceProvider* resource_provider_ = nullptr;
};

}  // namespace Hummingbird::Engine
