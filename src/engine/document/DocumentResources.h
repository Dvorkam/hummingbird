#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "engine/document/FontFaceResolver.h"

namespace Hummingbird::Layout {
class RenderObject;
}

namespace Hummingbird {
class IResourceProvider;
class IImageDecoder;
}  // namespace Hummingbird

namespace Hummingbird::Engine {

class ResourceStore;

class DocumentResources : public IFontFaceResolver {
public:
    DocumentResources(ResourceStore* resource_store, IResourceProvider* resource_provider, IImageDecoder* image_decoder)
        : resource_store_(resource_store), resource_provider_(resource_provider), image_decoder_(image_decoder) {}

    std::string build_css_source(std::string_view base_url, const std::vector<std::string>& style_blocks,
                                 const std::vector<std::string>& stylesheet_links,
                                 const std::vector<std::string>& extension_style_blocks = {}) const;
    bool update_image_resources(Layout::RenderObject* render_tree, std::string_view base_url) const;
    bool update_svg_resources(Layout::RenderObject* render_tree) const;

    Css::FontFaceRegistry resolve_font_faces(const std::vector<Css::FontFaceRule>& faces,
                                             std::vector<std::string>& out_pending_remote) const override;

private:
    ResourceStore* resource_store_ = nullptr;
    IResourceProvider* resource_provider_ = nullptr;
    IImageDecoder* image_decoder_ = nullptr;
};

}  // namespace Hummingbird::Engine
