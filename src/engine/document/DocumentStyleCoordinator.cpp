#include "engine/document/DocumentStyleCoordinator.h"

#include <string>
#include <utility>
#include <vector>

#include "engine/document/DocumentModel.h"
#include "engine/document/DocumentResources.h"

namespace Hummingbird::Engine {

DocumentStyleCoordinator::DocumentStyleCoordinator(DocumentModel& model, const DocumentResources& resources)
    : model_(model), resources_(resources) {}

void DocumentStyleCoordinator::set_extension_style_blocks(const std::vector<std::string>& style_blocks) {
    extension_style_blocks_ = style_blocks;
}

bool DocumentStyleCoordinator::apply_styles_and_build(std::string_view base_url, const Css::MediaContext& media) {
    std::vector<std::string> style_blocks = model_.style_blocks();
    std::string css =
        resources_.build_css_source(base_url, style_blocks, model_.stylesheet_links(), extension_style_blocks_);
    model_.apply_styles(css, media);
    return model_.build_render_tree();
}

bool DocumentStyleCoordinator::update_image_resources(std::string_view base_url) {
    bool updated = resources_.update_image_resources(model_.render_tree(), base_url);
    updated = resources_.update_svg_resources(model_.render_tree()) || updated;
    return updated;
}

}  // namespace Hummingbird::Engine
