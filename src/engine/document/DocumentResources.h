#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace Hummingbird::Layout {
class RenderObject;
}

namespace Hummingbird {
class IResourceProvider;
class IImageDecoder;
}  // namespace Hummingbird

namespace Hummingbird::Engine {

class ResourceStore;

class DocumentResources {
public:
    DocumentResources(ResourceStore* resource_store, IResourceProvider* resource_provider, IImageDecoder* image_decoder)
        : resource_store_(resource_store), resource_provider_(resource_provider), image_decoder_(image_decoder) {}

    std::string build_css_source(std::string_view base_url, const std::vector<std::string>& style_blocks,
                                 const std::vector<std::string>& stylesheet_links) const;
    bool update_image_resources(Layout::RenderObject* render_tree, std::string_view base_url) const;
    bool update_svg_resources(Layout::RenderObject* render_tree) const;

private:
    ResourceStore* resource_store_ = nullptr;
    IResourceProvider* resource_provider_ = nullptr;
    IImageDecoder* image_decoder_ = nullptr;
};

}  // namespace Hummingbird::Engine
