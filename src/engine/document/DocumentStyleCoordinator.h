#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "style/compute/Stylesheet.h"

namespace Hummingbird::Engine {

class DocumentModel;
class DocumentResources;

class DocumentStyleCoordinator {
public:
    DocumentStyleCoordinator(DocumentModel& model, const DocumentResources& resources);

    void set_extension_style_blocks(const std::vector<std::string>& style_blocks);
    bool apply_styles_and_build(std::string_view base_url, const Css::MediaContext& media);
    bool update_image_resources(std::string_view base_url);

private:
    DocumentModel& model_;
    const DocumentResources& resources_;
    std::vector<std::string> extension_style_blocks_;
};

}  // namespace Hummingbird::Engine
