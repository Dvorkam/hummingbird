#include "engine/document/DocumentResources.h"

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <ostream>

#include "core/dom/Element.h"
#include "core/dom/Text.h"
#include "core/platform_api/IImageDecoder.h"
#include "core/platform_api/IResourceProvider.h"
#include "core/utils/Log.h"
#include "engine/resources/ResourceStore.h"
#include "engine/resources/ResourceUrl.h"
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

// Blend2D decodes raw TrueType/OpenType only; WOFF/WOFF2/SVG/EOT need a
// decompressor we do not bundle yet (T-FONT-WOFF2-1). An empty format hint
// (extension-less url with no format()) is treated as loadable and left to the
// font cache to accept or reject.
bool is_loadable_font_format(const std::string& format) {
    return format.empty() || format == "truetype" || format == "opentype";
}

// A remote src that must be fetched over the network (has an explicit scheme).
// Everything else is treated as a bundled-asset-relative path resolved at paint
// time; root-relative ("/x") and protocol-relative ("//host/x") document-origin
// urls are not supported without a document base and are skipped by the caller.
bool is_remote_font_url(const std::string& url) {
    return url.find("://") != std::string::npos;
}

bool is_asset_relative_font_url(const std::string& url) {
    return !url.empty() && url.front() != '/' && url.front() != '\\' && url.find("://") == std::string::npos;
}

// Writes fetched font bytes to a stable per-url file under the OS temp dir and
// returns its path, so the existing file-based font cache can load it via
// createFromFile (remote and local sources converge on a filesystem path). The
// cache is content-addressed by url hash and written once; concurrent tabs and
// repeat navigations reuse it.
std::string write_font_cache_file(const std::string& url, std::string_view bytes) {
    std::error_code ec;
    std::filesystem::path dir = std::filesystem::temp_directory_path(ec) / "hummingbird" / "fonts";
    if (ec) {
        return {};
    }
    std::filesystem::create_directories(dir, ec);
    if (ec) {
        return {};
    }
    const std::uint64_t hash = std::hash<std::string>{}(url);
    char name[32];
    std::snprintf(name, sizeof(name), "%016llx.font", static_cast<unsigned long long>(hash));
    std::filesystem::path file = dir / name;

    if (!std::filesystem::exists(file)) {
        std::ofstream out(file, std::ios::binary | std::ios::trunc);
        if (!out) {
            return {};
        }
        out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        if (!out) {
            return {};
        }
    }
    return file.string();
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
                                                const std::vector<std::string>& stylesheet_links,
                                                const std::vector<std::string>& extension_style_blocks) const {
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
        return Css::merge_css_sources(ua_css, {}, style_blocks, extension_style_blocks);
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
        } else if (view->state == ResourceState::Blocked) {
            // Not counted as failed, and not warned about: a filter rule did
            // exactly what it was told to (story 9.4.1). Logged all the same,
            // because "the page looks wrong with the blocker on" needs to be
            // traceable to the stylesheet that did not load.
            HB_LOG_INFO("[resource] stylesheet blocked by filter: " << key);
        }
    }
    if (!stylesheet_links.empty()) {
        HB_LOG_DEBUG("[style] link stylesheets ready=" << ready_count << " loading=" << loading_count
                                                       << " failed=" << failed_count << " missing=" << missing_count);
    }

    return Css::merge_css_sources(ua_css, link_sources, style_blocks, extension_style_blocks);
}

// Binds each image-bearing render object to a resource HANDLE. Named
// bind_image_resources rather than update_*: since a handle is stable and
// resolves per paint, this is a one-shot binding after a render-tree rebuild,
// not the periodic re-pointing it used to be (T-RESOURCE-REF-1). The animation
// tick no longer calls it at all.
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
            ResourceRef background_ref{};
            if (style && style->background_image && !style->background_image->empty()) {
                auto resolved = resolve_resource_url(base_url, *style->background_image);
                // Bound by NAME, so this no longer waits for the bytes: a handle
                // minted before the resource is decoded simply resolves to null
                // until it arrives, and keeps working after the store replaces
                // or drops the payload.
                background_ref = resource_store_->ref_for(resolved.key, ResourceType::Image);
            }
            if (current.set_background_image(background_ref)) {
                changed = true;
            }
            if (auto* image = dynamic_cast<Layout::RenderImage*>(&current)) {
                const auto* element = static_cast<const DOM::Element*>(image->get_dom_node());
                ResourceRef image_ref{};
                if (const auto* src = element->find_attribute(Hummingbird::Html::AttributeNames::Src);
                    src && !src->empty()) {
                    auto resolved = resolve_resource_url(base_url, *src);
                    image_ref = resource_store_->ref_for(resolved.key, ResourceType::Image);
                }
                if (image->set_image(image_ref)) {
                    changed = true;
                }
            }
            return Layout::Traversal::TraverseAction::Continue;
        });

    return changed;
}

Css::FontFaceRegistry DocumentResources::resolve_font_faces(const std::vector<Css::FontFaceRule>& faces,
                                                            std::vector<std::string>& out_pending_remote) const {
    Css::FontFaceRegistry registry;
    for (const auto& face : faces) {
        if (face.family.empty() || face.sources.empty()) {
            continue;
        }
        // Prefer the first source we can actually decode.
        const Css::FontFaceSource* chosen = nullptr;
        for (const auto& source : face.sources) {
            if (is_loadable_font_format(source.format)) {
                chosen = &source;
                break;
            }
        }
        if (!chosen) {
            HB_LOG_DEBUG("[font] @font-face '" << face.family << "' has no decodable source (WOFF2 needs "
                                               << "T-FONT-WOFF2-1); falling back");
            continue;
        }
        const std::string& url = chosen->url;

        if (is_remote_font_url(url)) {
            if (resource_store_) {
                auto view = resource_store_->view(url, ResourceType::Font);
                if (view && view->state == ResourceState::Ready && !view->body.empty()) {
                    std::string path = write_font_cache_file(url, view->body);
                    if (!path.empty()) {
                        registry.register_family(face.family, std::move(path));
                    }
                    continue;
                }
                // Both are terminal answers, so neither may be re-requested.
                // Blocked especially: this runs on every style resolve, so
                // leaving it out would re-request a filtered font continuously
                // for as long as the page is open.
                if (view && (view->state == ResourceState::Failed || view->state == ResourceState::Blocked)) {
                    continue;
                }
            }
            // Not fetched yet: ask the caller to request it, then re-resolve on
            // the rebuild triggered when the bytes arrive.
            out_pending_remote.push_back(url);
        } else if (is_asset_relative_font_url(url)) {
            // Bundled asset: resolve_asset_path_string finds the file at paint.
            registry.register_family(face.family, url);
        }
        // else: document-origin (root/protocol-relative) url without a base;
        // unsupported for now — leave the family unregistered (text falls back).
    }
    return registry;
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
