#include "engine/document/DocumentResources.h"

#include <ostream>

#include "core/dom/Element.h"
#include "core/dom/Text.h"
#include "core/platform_api/IImageDecoder.h"
#include "core/platform_api/IResourceProvider.h"
#include "core/utils/Log.h"
#include "engine/ResourceUrl.h"
#include "engine/resources/ResourceStore.h"
#include "html/HtmlAttributeNames.h"
#include "html/HtmlTagNames.h"
#include "layout/RenderObject.h"
#include "layout/paint/RenderTreeTraversal.h"
#include "layout/replaced/RenderImage.h"
#include "layout/replaced/RenderSvg.h"
#include "style/compute/StylesheetSource.h"

namespace Hummingbird {
struct ImageBitmap;
}  // namespace Hummingbird

namespace Hummingbird::Engine {

namespace {
void append_escaped(std::string& out, std::string_view text, bool attribute) {
    for (char ch : text) {
        switch (ch) {
            case '&':
                out.append("&amp;");
                break;
            case '<':
                out.append("&lt;");
                break;
            case '>':
                out.append("&gt;");
                break;
            case '"':
                if (attribute) {
                    out.append("&quot;");
                } else {
                    out.push_back(ch);
                }
                break;
            case '\'':
                if (attribute) {
                    out.append("&apos;");
                } else {
                    out.push_back(ch);
                }
                break;
            default:
                out.push_back(ch);
                break;
        }
    }
}

void serialize_svg_node(const DOM::Node* node, std::string& out, bool is_root) {
    if (!node) {
        return;
    }

    if (auto text_node = dynamic_cast<const DOM::Text*>(node)) {
        append_escaped(out, text_node->get_text(), false);
        return;
    }

    auto element = dynamic_cast<const DOM::Element*>(node);
    if (!element) {
        return;
    }

    const auto& tag = element->get_tag_name();
    out.push_back('<');
    out.append(tag);

    bool has_xmlns = false;
    for (const auto& [key, value] : element->get_attributes()) {
        if (key == "xmlns") {
            has_xmlns = true;
        }
        out.push_back(' ');
        out.append(key);
        out.append("=\"");
        append_escaped(out, value, true);
        out.push_back('"');
    }
    if (is_root && tag == Hummingbird::Html::TagNames::Svg && !has_xmlns) {
        out.append(" xmlns=\"http://www.w3.org/2000/svg\"");
    }

    const auto& children = element->get_children();
    if (children.empty()) {
        out.append("/>");
        return;
    }

    out.push_back('>');
    for (const auto& child : children) {
        serialize_svg_node(child.get(), out, false);
    }
    out.append("</");
    out.append(tag);
    out.push_back('>');
}
}  // namespace

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
            const auto* style = current.get_computed_style();
            const ImageBitmap* background_bitmap = nullptr;
            if (style && style->background_image && !style->background_image->empty()) {
                auto resolved = resolve_resource_url(base_url, *style->background_image);
                const std::string& key = resolved.key;
                auto view = resource_store_->view(key, ResourceType::Image);
                if (view && view->state == ResourceState::Ready) {
                    background_bitmap = view->image;
                }
            }
            if (current.set_background_image(background_bitmap)) {
                changed = true;
            }
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

bool DocumentResources::update_svg_resources(Layout::RenderObject* render_tree) const {
    if (!render_tree || !image_decoder_) {
        return false;
    }

    bool changed = false;
    Layout::Point offset{0.0f, 0.0f};
    Layout::Traversal::traverse_render_tree(
        *render_tree, offset,
        [&](Layout::RenderObject& current, const Layout::Rect& /*absolute*/, const Layout::Point& /*local_offset*/) {
            auto* svg = dynamic_cast<Layout::RenderSvg*>(&current);
            if (!svg) {
                return Layout::Traversal::TraverseAction::Continue;
            }
            auto* element = static_cast<const DOM::Element*>(svg->get_dom_node());
            if (!element) {
                return Layout::Traversal::TraverseAction::Continue;
            }

            std::string markup;
            serialize_svg_node(element, markup, true);
            std::unique_ptr<ImageBitmap> decoded_bitmap;
            if (!markup.empty()) {
                if (auto decoded = image_decoder_->decode(markup)) {
                    decoded_bitmap = std::make_unique<ImageBitmap>(std::move(*decoded));
                }
            }

            if (svg->set_image(std::move(decoded_bitmap))) {
                changed = true;
            }
            return Layout::Traversal::TraverseAction::Continue;
        });

    return changed;
}

}  // namespace Hummingbird::Engine
