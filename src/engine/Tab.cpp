#include "engine/Tab.h"

#include <algorithm>
#include <optional>
#include <utility>

#include "core/dom/Element.h"
#include "core/dom/Node.h"
#include "core/utils/Log.h"
#include "core/utils/Timing.h"
#include "core/utils/Url.h"
#include "html/HtmlAttributeNames.h"
#include "html/HtmlParser.h"
#include "html/HtmlTagNames.h"
#include "layout/RenderImage.h"
#include "layout/RenderObject.h"
#include "style/CssParser.h"
#include "style/StylesheetSource.h"

namespace Hummingbird::Engine {

namespace {
size_t count_nodes_recursive(const DOM::Node* node) {
    if (!node) return 0;
    size_t total = 1;
    for (const auto& child : node->get_children()) {
        total += count_nodes_recursive(child.get());
    }
    return total;
}

void collect_image_sources(const DOM::Node* node, std::vector<std::string>& out) {
    if (!node) return;
    if (auto* element = dynamic_cast<const DOM::Element*>(node)) {
        if (element->get_tag_name() == Hummingbird::Html::TagNames::Img) {
            const auto& attrs = element->get_attributes();
            static const std::string kSrcKey = std::string(Hummingbird::Html::AttributeNames::Src);
            auto it = attrs.find(kSrcKey);
            if (it != attrs.end() && !it->second.empty()) {
                out.push_back(it->second);
            }
        }
    }
    for (const auto& child : node->get_children()) {
        collect_image_sources(child.get(), out);
    }
}

bool point_in_rect(const Layout::Rect& rect, const Layout::Point& point) {
    if (rect.width <= 0.0f || rect.height <= 0.0f) return false;
    return point.x >= rect.x && point.x <= rect.x + rect.width && point.y >= rect.y && point.y <= rect.y + rect.height;
}

bool intersects(const Layout::Rect& a, const Layout::Rect& b) {
    if (a.width <= 0.0f || a.height <= 0.0f) return false;
    if (b.width <= 0.0f || b.height <= 0.0f) return false;
    return !(a.x + a.width <= b.x || a.x >= b.x + b.width || a.y + a.height <= b.y || a.y >= b.y + b.height);
}

struct ResolvedUrl {
    std::string resolved;
    std::string_view key;
};

ResolvedUrl resolve_resource_url(std::string_view base_url, std::string_view raw_url) {
    ResolvedUrl result;
    result.resolved = Core::resolve_url(base_url, raw_url);
    result.key = result.resolved.empty() ? raw_url : std::string_view(result.resolved);
    return result;
}

std::optional<std::string> resolve_anchor_href(const DOM::Node* node, std::string_view base_url) {
    static const std::string kHrefKey = std::string(Hummingbird::Html::AttributeNames::Href);
    const DOM::Node* current = node;
    while (current) {
        auto* element = dynamic_cast<const DOM::Element*>(current);
        if (element && element->get_tag_name() == Hummingbird::Html::TagNames::A) {
            const auto& attrs = element->get_attributes();
            auto it = attrs.find(kHrefKey);
            if (it == attrs.end() || it->second.empty()) {
                return std::nullopt;
            }
            auto resolved = resolve_resource_url(base_url, it->second);
            return resolved.resolved.empty() ? std::optional<std::string>(it->second)
                                             : std::optional<std::string>(std::move(resolved.resolved));
        }
        current = current->get_parent();
    }
    return std::nullopt;
}

std::optional<std::string> hit_test_link_recursive(const Layout::RenderObject& node, const Layout::Point& offset,
                                                   const Layout::Point& point, const Layout::Rect& viewport,
                                                   std::string_view base_url) {
    const auto& rect = node.get_rect();
    Layout::Rect absolute{offset.x + rect.x, offset.y + rect.y, rect.width, rect.height};
    if (!intersects(absolute, viewport)) {
        return std::nullopt;
    }
    if (!point_in_rect(absolute, point)) {
        return std::nullopt;
    }

    const auto& children = node.get_children();
    for (auto it = children.rbegin(); it != children.rend(); ++it) {
        Layout::Point child_offset{absolute.x, absolute.y};
        if (auto hit = hit_test_link_recursive(**it, child_offset, point, viewport, base_url)) {
            return hit;
        }
    }

    return resolve_anchor_href(node.get_dom_node(), base_url);
}
}  // namespace

Tab::Tab(NetworkPtr network, NetworkPtr fallback_network, ResourceProviderPtr resource_provider,
         ImageDecoderPtr image_decoder)
    : network_(std::move(network)),
      fallback_network_(std::move(fallback_network)),
      resource_provider_(std::move(resource_provider)),
      image_decoder_(std::move(image_decoder)) {
    if (!network_ || !fallback_network_) {
        HB_LOG_ERROR("[network] failed to create network backend(s)");
    }
    if (!resource_provider_) {
        HB_LOG_WARN("[resource] no resource provider available");
    }
    if (!image_decoder_) {
        HB_LOG_WARN("[image] no decoder available");
    }
}

Tab::~Tab() {
    shutdown();
}

void Tab::shutdown() {
    if (shutting_down_.exchange(true, std::memory_order_relaxed)) return;

    active_nav_.store(UINT64_MAX, std::memory_order_release);

    if (network_) network_->shutdown();
    if (fallback_network_) fallback_network_->shutdown();

    {
        std::lock_guard<std::mutex> lg(pending_mutex_);
        pending_resources_.clear();
    }
}

void Tab::navigate(std::string_view url) {
    if (shutting_down_.load(std::memory_order_relaxed)) return;

    const uint64_t id = ++nav_counter_;
    active_nav_.store(id, std::memory_order_release);
    std::string normalized = Core::normalize_input_url(url);
    std::string url_copy = normalized;
    requested_url_ = std::move(normalized);
    reset_document_state();
    if (!resource_store_.begin_request(url_copy, ResourceType::Document)) {
        HB_LOG_WARN("[resource] failed to register document request: " << url_copy);
    }

    // Built-in demo URL: keep startup deterministic and avoid network timeouts.
    if ((url_copy == "http://example.dev" || url_copy == "https://example.dev") && fallback_network_) {
        fallback_network_->get(url_copy, [this, id, url_copy](NetworkResponse response) {
            if (id != active_nav_.load(std::memory_order_acquire)) return;
            bool success = !response.body.empty();
            enqueue_resource_update(ResourceType::Document, url_copy, std::move(response.body), success,
                                    std::move(response.effective_url));
        });
        return;
    }

    if (!network_) {
        HB_LOG_ERROR("[network] no backend available for " << url);
        if (fallback_network_) {
            fallback_network_->get(url_copy, [this, id, url_copy](NetworkResponse response) {
                if (id != active_nav_.load(std::memory_order_acquire)) return;
                bool success = !response.body.empty();
                enqueue_resource_update(ResourceType::Document, url_copy, std::move(response.body), success,
                                        std::move(response.effective_url));
            });
        } else {
            enqueue_resource_update(ResourceType::Document, url_copy, {}, false);
        }
        return;
    }

    network_->get(url_copy, [this, id, url_copy](NetworkResponse response) {
        if (id != active_nav_.load(std::memory_order_acquire)) return;

        if (response.body.empty()) {
            HB_LOG_WARN("[network] curl returned empty for " << url_copy << ", using stub");
            if (!fallback_network_) return;
            fallback_network_->get(url_copy, [this, id, url_copy](NetworkResponse fallback) {
                if (id != active_nav_.load(std::memory_order_acquire)) return;
                bool success = !fallback.body.empty();
                enqueue_resource_update(ResourceType::Document, url_copy, std::move(fallback.body), success,
                                        std::move(fallback.effective_url));
            });
            return;
        }

        HB_LOG_INFO("[network] fetched " << response.body.size() << " bytes from " << url_copy);
        enqueue_resource_update(ResourceType::Document, url_copy, std::move(response.body), true,
                                std::move(response.effective_url));
    });
}

bool Tab::tick(IGraphicsContext& graphics, const Layout::Rect& viewport) {
    if (shutting_down_.load(std::memory_order_relaxed)) return false;

    consume_pending_resources(graphics, viewport);

    if (render_tree_) {
        bool viewport_changed = !has_viewport_ || viewport.x != last_viewport_.x || viewport.y != last_viewport_.y ||
                                viewport.width != last_viewport_.width || viewport.height != last_viewport_.height;
        if (viewport_changed) {
            relayout(graphics, viewport);
        }
    }

    bool dirty = dirty_;
    dirty_ = false;
    return dirty;
}

void Tab::paint(IGraphicsContext& graphics, const Layout::Rect& viewport, bool debug_outlines) {
    if (!render_tree_) return;

    graphics.set_viewport(viewport);

    Renderer::PaintOptions opts;
    opts.debug_outlines = debug_outlines;
    opts.scroll_y = scroll_y_;
    opts.viewport = viewport;

    const auto paint_start = Core::Clock::now();
    painter_.paint(*render_tree_, graphics, opts);
    const auto paint_end = Core::Clock::now();
    static int paint_log_counter = 0;
    if (++paint_log_counter % 5 == 0) {
        HB_LOG_DEBUG("[perf] paint ms=" << Core::duration_ms(paint_start, paint_end) << " scroll_y=" << scroll_y_);
    }
}

std::optional<std::string> Tab::hit_test_link(const Layout::Point& point, const Layout::Rect& viewport) const {
    if (!render_tree_) {
        return std::nullopt;
    }
    if (!point_in_rect(viewport, point)) {
        return std::nullopt;
    }
    Layout::Point offset{0.0f, -scroll_y_};
    return hit_test_link_recursive(*render_tree_, offset, point, viewport, requested_url_);
}

std::optional<ResourceView> Tab::resource_view(std::string_view url, ResourceType type) const {
    return resource_store_.view(url, type);
}

void Tab::scroll_by(float delta_px, float viewport_height) {
    scroll_y_ -= delta_px;
    clamp_scroll(viewport_height);
    dirty_ = true;
}

void Tab::clamp_scroll(float viewport_height) {
    const float max_scroll = std::max(0.0f, content_height_ - viewport_height);
    scroll_y_ = std::clamp(scroll_y_, 0.0f, max_scroll);
}

void Tab::consume_pending_resources(IGraphicsContext& graphics, const Layout::Rect& viewport) {
    auto pending = take_pending_resources();
    if (pending.empty()) return;

    const auto process_start = Core::Clock::now();
    const size_t pending_count = pending.size();
    bool document_ready = false;
    std::string document_url;
    bool stylesheet_ready = false;
    bool image_ready = false;
    size_t image_decode_count = 0;
    double image_decode_ms = 0.0;

    for (auto& update : pending) {
        if (update.success) {
            if (update.type == ResourceType::Document) {
                if (!update.effective_url.empty()) {
                    requested_url_ = update.effective_url;
                }
                resource_store_.mark_ready(update.url, update.type, std::move(update.body));
                document_ready = true;
                document_url = update.url;
            } else if (update.type == ResourceType::Stylesheet) {
                resource_store_.mark_ready(update.url, update.type, std::move(update.body));
                stylesheet_ready = true;
            } else if (update.type == ResourceType::Image) {
                if (!image_decoder_) {
                    HB_LOG_WARN("[image] decode skipped (no decoder): " << update.url);
                    resource_store_.mark_failed(update.url, update.type);
                    continue;
                }
                const auto decode_start = Core::Clock::now();
                auto decoded = image_decoder_->decode(update.body);
                const auto decode_end = Core::Clock::now();
                image_decode_ms += Core::duration_ms(decode_start, decode_end);
                ++image_decode_count;
                if (!decoded) {
                    HB_LOG_WARN("[image] decode failed: " << update.url);
                    resource_store_.mark_failed(update.url, update.type);
                    continue;
                }
                resource_store_.mark_ready(update.url, update.type, std::move(update.body));
                resource_store_.set_image(update.url, update.type, std::move(*decoded));
                image_ready = true;
            }
        } else {
            resource_store_.mark_failed(update.url, update.type);
            if (update.type == ResourceType::Document) {
                HB_LOG_WARN("[resource] document failed to load: " << update.url);
            }
        }
    }

    const auto process_end = Core::Clock::now();
    HB_LOG_INFO("[perf] resource batch ms="
                << Core::duration_ms(process_start, process_end) << " pending=" << pending_count
                << " doc=" << static_cast<int>(document_ready) << " styles=" << static_cast<int>(stylesheet_ready)
                << " images=" << static_cast<int>(image_ready) << " decode_ms=" << image_decode_ms
                << " decoded_images=" << image_decode_count);

    if (document_ready) {
        const auto* entry = resource_store_.find(document_url, ResourceType::Document);
        if (entry) {
            const auto rebuild_start = Core::Clock::now();
            rebuild_from_html(graphics, viewport, entry->body);
            const auto rebuild_end = Core::Clock::now();
            HB_LOG_INFO("[perf] document rebuild ms=" << Core::duration_ms(rebuild_start, rebuild_end));
        }
    } else if (stylesheet_ready && dom_tree_) {
        const auto style_update_start = Core::Clock::now();
        apply_styles_and_layout(graphics, viewport);
        const auto style_update_end = Core::Clock::now();
        HB_LOG_INFO("[perf] stylesheet update ms=" << Core::duration_ms(style_update_start, style_update_end));
    } else if (image_ready && render_tree_) {
        const auto image_update_start = Core::Clock::now();
        bool updated = update_image_resources();
        if (updated) {
            relayout(graphics, viewport);
        }
        const auto image_update_end = Core::Clock::now();
        HB_LOG_INFO("[perf] image update ms=" << Core::duration_ms(image_update_start, image_update_end)
                                              << " updated=" << updated);
        dirty_ = true;
    }
}

void Tab::enqueue_resource_update(ResourceType type, std::string url, std::string body, bool success,
                                  std::string effective_url) {
    std::lock_guard<std::mutex> lg(pending_mutex_);
    pending_resources_.push_back({type, std::move(url), std::move(effective_url), std::move(body), success});
}

std::vector<Tab::PendingResourceUpdate> Tab::take_pending_resources() {
    std::vector<PendingResourceUpdate> pending;
    std::lock_guard<std::mutex> lg(pending_mutex_);
    pending.swap(pending_resources_);
    return pending;
}

void Tab::rebuild_from_html(IGraphicsContext& graphics, const Layout::Rect& viewport, const std::string& html) {
    HB_LOG_INFO("[pipeline] html size: " << html.size());

    std::vector<std::string> style_blocks;
    std::vector<std::string> stylesheet_links;
    std::vector<std::string> image_links;
    if (!parse_html(html, style_blocks, stylesheet_links, image_links)) {
        return;
    }
    style_blocks_ = std::move(style_blocks);
    stylesheet_links_ = std::move(stylesheet_links);
    image_links_ = std::move(image_links);
    if (!stylesheet_links.empty()) {
        HB_LOG_INFO("[pipeline] discovered stylesheet links: " << stylesheet_links.size());
    }
    if (!image_links_.empty()) {
        HB_LOG_INFO("[pipeline] discovered image sources: " << image_links_.size());
    }

    request_stylesheets(stylesheet_links_);
    request_images(image_links_);
    apply_styles_and_layout(graphics, viewport);
    HB_LOG_INFO("[pipeline] render tree root children: " << render_tree_->get_children().size());
    dirty_ = true;
}

void Tab::reset_document_state() {
    dom_tree_.reset();
    render_tree_.reset();
    dom_arena_.reset();
    scroll_y_ = 0.0f;
    content_height_ = 0.0f;
    has_viewport_ = false;
    dirty_ = true;
    resource_store_.clear();
    style_blocks_.clear();
    stylesheet_links_.clear();
    image_links_.clear();
}

bool Tab::parse_html(const std::string& html, std::vector<std::string>& style_blocks,
                     std::vector<std::string>& stylesheet_links, std::vector<std::string>& image_links) {
    const auto parse_start = Core::Clock::now();
    Html::Parser parser(dom_arena_, html);
    auto parse_result = parser.parse();
    const auto parse_end = Core::Clock::now();

    dom_tree_ = std::move(parse_result.dom);
    style_blocks = std::move(parse_result.style_blocks);
    stylesheet_links = std::move(parse_result.stylesheet_links);

    if (!dom_tree_) {
        HB_LOG_WARN("[pipeline] parsed empty DOM");
        return false;
    }

    HB_LOG_INFO("[pipeline] parsed DOM children: " << dom_tree_->get_children().size()
                                                   << " total nodes: " << count_nodes_recursive(dom_tree_.get()));
    HB_LOG_INFO("[perf] html parse ms=" << Core::duration_ms(parse_start, parse_end));

    image_links.clear();
    collect_image_sources(dom_tree_.get(), image_links);
    return true;
}

void Tab::request_stylesheets(const std::vector<std::string>& stylesheet_links) {
    ResourceRequestOptions options{};
    options.type = ResourceType::Stylesheet;
    options.type_label = "stylesheet";
    options.attr_label = "href";
    options.allow_fallback_network = false;
    options.log_duplicates = true;
    options.log_asset_load = true;
    options.mark_ready_on_asset = true;

    request_resources(stylesheet_links, options);
}

void Tab::request_images(const std::vector<std::string>& image_links) {
    ResourceRequestOptions options{};
    options.type = ResourceType::Image;
    options.type_label = "image";
    options.attr_label = "src";
    options.allow_fallback_network = true;
    options.log_duplicates = false;
    options.log_asset_load = false;
    options.mark_ready_on_asset = false;

    request_resources(image_links, options);
}

void Tab::request_resources(const std::vector<std::string>& links, const ResourceRequestOptions& options) {
    if (links.empty()) return;

    const uint64_t nav_id = active_nav_.load(std::memory_order_acquire);
    const std::string type_label(options.type_label);

    for (const auto& raw_url : links) {
        auto resolved = resolve_resource_url(requested_url_, raw_url);
        std::string url(resolved.key);
        if (url.empty()) continue;

        if (!resource_store_.begin_request(url, options.type)) {
            if (options.log_duplicates) {
                HB_LOG_DEBUG("[resource] " << options.type_label << " already requested: " << url);
            }
            continue;
        }

        HB_LOG_DEBUG("[resource] " << options.type_label << " link: " << options.attr_label << "=" << raw_url
                                   << " base=" << requested_url_ << " resolved=" << url);
        if (url.find("://") == std::string::npos) {
            HB_LOG_WARN("[resource] " << options.type_label << " resolved to non-absolute url: " << url
                                      << " base=" << requested_url_);
        }

        if (resource_provider_) {
            auto data = resource_provider_->load_text(raw_url);
            if (!data && !resolved.resolved.empty() && resolved.resolved != raw_url) {
                data = resource_provider_->load_text(resolved.resolved);
            }
            if (data) {
                if (options.log_asset_load) {
                    HB_LOG_DEBUG("[resource] " << options.type_label << " loaded from assets: " << url
                                               << " bytes=" << data->size());
                }
                if (options.mark_ready_on_asset) {
                    resource_store_.mark_ready(url, options.type, std::move(*data));
                } else {
                    enqueue_resource_update(options.type, url, std::move(*data), true);
                }
                continue;
            }
        }

        INetwork* fetcher = network_.get();
        if (!fetcher && options.allow_fallback_network) {
            fetcher = fallback_network_.get();
        }
        if (!fetcher) {
            HB_LOG_WARN("[resource] no network for " << options.type_label << ": " << url);
            resource_store_.mark_failed(url, options.type);
            continue;
        }

        HB_LOG_DEBUG("[resource] fetching " << options.type_label << ": " << url);
        fetcher->get(url, [this, nav_id, url, type = options.type, type_label](NetworkResponse response) {
            if (nav_id != active_nav_.load(std::memory_order_acquire)) return;
            bool success = !response.body.empty();
            if (!success) {
                HB_LOG_WARN("[resource] " << type_label << " fetch failed: " << url);
            }
            enqueue_resource_update(type, url, std::move(response.body), success);
        });
    }
}

void Tab::apply_styles_and_layout(IGraphicsContext& graphics, const Layout::Rect& viewport) {
    std::string css = build_css_source(style_blocks_, stylesheet_links_);
    parse_and_apply_css(css);

    if (!build_render_tree()) {
        return;
    }

    update_image_resources();
    relayout(graphics, viewport);
}

std::string Tab::build_css_source(const std::vector<std::string>& style_blocks,
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

    std::vector<std::string> link_sources;
    link_sources.reserve(stylesheet_links.size());
    size_t ready_count = 0;
    size_t loading_count = 0;
    size_t missing_count = 0;
    size_t failed_count = 0;
    for (const auto& href : stylesheet_links) {
        auto resolved = resolve_resource_url(requested_url_, href);
        std::string_view key = resolved.key;
        auto view = resource_store_.view(key, ResourceType::Stylesheet);
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

void Tab::parse_and_apply_css(const std::string& css) {
    const auto css_parse_start = Core::Clock::now();
    Css::Parser css_parser(css);
    auto stylesheet = css_parser.parse();
    const auto css_parse_end = Core::Clock::now();
    HB_LOG_INFO("[perf] css parse ms=" << Core::duration_ms(css_parse_start, css_parse_end)
                                       << " rules=" << stylesheet.rules.size());

    const auto style_start = Core::Clock::now();
    style_engine_.apply(stylesheet, dom_tree_.get());
    const auto style_end = Core::Clock::now();
    HB_LOG_INFO("[pipeline] applied stylesheet rules: " << stylesheet.rules.size());
    HB_LOG_INFO("[perf] style apply ms=" << Core::duration_ms(style_start, style_end));
}

bool Tab::build_render_tree() {
    const auto render_start = Core::Clock::now();
    render_tree_ = tree_builder_.build(dom_tree_.get());
    const auto render_end = Core::Clock::now();
    if (!render_tree_) {
        HB_LOG_WARN("[pipeline] render tree build skipped");
        return false;
    }
    HB_LOG_INFO("[perf] render tree build ms=" << Core::duration_ms(render_start, render_end));
    return true;
}

bool Tab::update_image_resources() {
    if (!render_tree_) {
        return false;
    }

    bool changed = false;
    std::vector<Layout::RenderObject*> stack;
    stack.push_back(render_tree_.get());

    while (!stack.empty()) {
        Layout::RenderObject* current = stack.back();
        stack.pop_back();

        if (auto* image = dynamic_cast<Layout::RenderImage*>(current)) {
            const auto* element = static_cast<const DOM::Element*>(image->get_dom_node());
            const auto& attrs = element->get_attributes();
            static const std::string kSrcKey = std::string(Hummingbird::Html::AttributeNames::Src);
            auto it = attrs.find(kSrcKey);
            const ImageBitmap* bitmap = nullptr;
            if (it != attrs.end() && !it->second.empty()) {
                auto resolved = resolve_resource_url(requested_url_, it->second);
                std::string_view key = resolved.key;
                auto view = resource_store_.view(key, ResourceType::Image);
                if (view && view->state == ResourceState::Ready) {
                    bitmap = view->image;
                }
            }
            if (image->set_image(bitmap)) {
                changed = true;
            }
        }

        for (const auto& child : current->get_children()) {
            stack.push_back(child.get());
        }
    }

    return changed;
}

void Tab::relayout(IGraphicsContext& graphics, const Layout::Rect& viewport) {
    if (!render_tree_) return;

    const auto layout_start = Core::Clock::now();
    render_tree_->layout(graphics, viewport);
    const auto layout_end = Core::Clock::now();
    content_height_ = render_tree_->get_rect().height;
    last_viewport_ = viewport;
    has_viewport_ = true;
    clamp_scroll(viewport.height);
    HB_LOG_INFO("[perf] layout ms=" << Core::duration_ms(layout_start, layout_end) << " viewport=" << viewport.width
                                    << "x" << viewport.height);
    dirty_ = true;
}

}  // namespace Hummingbird::Engine
