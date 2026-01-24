#include "engine/DocumentResources.h"

#include <ostream>

#include "core/dom/Element.h"
#include "core/platform_api/IResourceProvider.h"
#include "core/utils/Log.h"
#include "engine/ResourceStore.h"
#include "engine/ResourceUrl.h"
#include "html/HtmlAttributeNames.h"
#include "layout/RenderImage.h"
#include "layout/RenderObject.h"
#include "layout/RenderTreeTraversal.h"
#include "style/StylesheetSource.h"

namespace Hummingbird {
struct ImageBitmap;
}  // namespace Hummingbird

namespace Hummingbird::Engine {

std::string DocumentResources::build_css_source(std::string_view base_url, const std::vector<std::string>& style_blocks,
                                                const std::vector<std::string>& stylesheet_links) const {
    std::string ua_css;
    if (resource_provider_) {
        if (auto ua = resource_provider_->load_text("assets/ua.css")) {
            ua_css = std::move(*ua);
        }
    }
    if (ua_css.empty()) {
        ua_css = "body { padding: 8px; } p { margin: 4px; }";
    }

    if (!resource_store_) {
        return Css::merge_css_sources(ua_css, {}, style_blocks);
    }

    std::vector<std::string> link_sources;
    link_sources.reserve(stylesheet_links.size());
    size_t ready_count = 0;
    size_t loading_count = 0;
    size_t missing_count = 0;
    size_t failed_count = 0;
    for (const auto& href : stylesheet_links) {
        auto resolved = resolve_resource_url(base_url, href);
        const std::string& key = resolved.key;
        auto view = resource_store_->view(key, ResourceType::Stylesheet);
        if (!view) {
            ++missing_count;
            HB_LOG_DEBUG("[resource] stylesheet not in store: " << key);
            continue;
        }
        if (view->state == ResourceState::Ready) {
            link_sources.emplace_back(view->body);
            ++ready_count;
        } else if (view->state == ResourceState::Loading || view->state == ResourceState::Requested) {
            ++loading_count;
            HB_LOG_DEBUG("[resource] stylesheet pending: " << key);
        } else if (view->state == ResourceState::Failed) {
            ++failed_count;
            HB_LOG_WARN("[resource] missing stylesheet: " << key);
        }
    }
    if (!stylesheet_links.empty()) {
        HB_LOG_DEBUG("[style] link stylesheets ready=" << ready_count << " loading=" << loading_count
                                                       << " failed=" << failed_count << " missing=" << missing_count);
    }

    return Css::merge_css_sources(ua_css, link_sources, style_blocks);
}

bool DocumentResources::update_image_resources(Layout::RenderObject* render_tree, std::string_view base_url) const {
    if (!render_tree || !resource_store_) {
        return false;
    }

    bool changed = false;
    Layout::Point offset{0.0f, 0.0f};
    Layout::Traversal::traverse_render_tree(
        *render_tree, offset,
        [&](Layout::RenderObject& current, const Layout::Rect& /*absolute*/, const Layout::Point& /*local_offset*/) {
            if (auto* image = dynamic_cast<Layout::RenderImage*>(&current)) {
                const auto* element = static_cast<const DOM::Element*>(image->get_dom_node());
                const ImageBitmap* bitmap = nullptr;
                if (const auto* src = element->find_attribute(Hummingbird::Html::AttributeNames::Src);
                    src && !src->empty()) {
                    auto resolved = resolve_resource_url(base_url, *src);
                    const std::string& key = resolved.key;
                    auto view = resource_store_->view(key, ResourceType::Image);
                    if (view && view->state == ResourceState::Ready) {
                        bitmap = view->image;
                    }
                }
                if (image->set_image(bitmap)) {
                    changed = true;
                }
            }
            return Layout::Traversal::TraverseAction::Continue;
        });

    return changed;
}

}  // namespace Hummingbird::Engine
