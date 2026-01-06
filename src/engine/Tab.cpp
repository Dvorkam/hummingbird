#include "engine/Tab.h"

#include <algorithm>
#include <utility>

#include "core/dom/Node.h"
#include "core/utils/Log.h"
#include "core/utils/Timing.h"
#include "html/HtmlParser.h"
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
}  // namespace

Tab::Tab(NetworkPtr network, NetworkPtr fallback_network, ResourceProviderPtr resource_provider)
    : network_(std::move(network)),
      fallback_network_(std::move(fallback_network)),
      resource_provider_(std::move(resource_provider)) {
    if (!network_ || !fallback_network_) {
        HB_LOG_ERROR("[network] failed to create network backend(s)");
    }
    if (!resource_provider_) {
        HB_LOG_WARN("[resource] no resource provider available");
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
    std::string url_copy(url);
    requested_url_ = url_copy;
    reset_document_state();
    resource_store_.request(url_copy, ResourceType::Document);
    if (!resource_store_.mark_loading(url_copy, ResourceType::Document)) {
        HB_LOG_WARN("[resource] failed to register document request: " << url_copy);
    }

    // Built-in demo URL: keep startup deterministic and avoid network timeouts.
    if ((url == "http://example.dev" || url == "https://example.dev") && fallback_network_) {
        fallback_network_->get(url_copy, [this, id, url_copy](std::string body) {
            if (id != active_nav_.load(std::memory_order_acquire)) return;
            bool success = !body.empty();
            enqueue_resource_update(ResourceType::Document, url_copy, std::move(body), success);
        });
        return;
    }

    if (!network_) {
        HB_LOG_ERROR("[network] no backend available for " << url);
        if (fallback_network_) {
            fallback_network_->get(url_copy, [this, id, url_copy](std::string body) {
                if (id != active_nav_.load(std::memory_order_acquire)) return;
                bool success = !body.empty();
                enqueue_resource_update(ResourceType::Document, url_copy, std::move(body), success);
            });
        } else {
            enqueue_resource_update(ResourceType::Document, url_copy, {}, false);
        }
        return;
    }

    network_->get(url_copy, [this, id, url_copy](std::string body) {
        if (id != active_nav_.load(std::memory_order_acquire)) return;

        if (body.empty()) {
            HB_LOG_WARN("[network] curl returned empty for " << url_copy << ", using stub");
            if (!fallback_network_) return;
            fallback_network_->get(url_copy, [this, id, url_copy](std::string fallback) {
                if (id != active_nav_.load(std::memory_order_acquire)) return;
                bool success = !fallback.empty();
                enqueue_resource_update(ResourceType::Document, url_copy, std::move(fallback), success);
            });
            return;
        }

        HB_LOG_INFO("[network] fetched " << body.size() << " bytes from " << url_copy);
        enqueue_resource_update(ResourceType::Document, url_copy, std::move(body), true);
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

    for (auto& update : pending) {
        if (update.success) {
            resource_store_.mark_ready(update.url, update.type, std::move(update.body));
            if (update.type == ResourceType::Document) {
                const auto* entry = resource_store_.find(update.url, update.type);
                if (entry) {
                    rebuild_from_html(graphics, viewport, entry->body);
                }
            }
        } else {
            resource_store_.mark_failed(update.url, update.type);
            if (update.type == ResourceType::Document) {
                HB_LOG_WARN("[resource] document failed to load: " << update.url);
            }
        }
    }
}

void Tab::enqueue_resource_update(ResourceType type, std::string url, std::string body, bool success) {
    std::lock_guard<std::mutex> lg(pending_mutex_);
    pending_resources_.push_back({type, std::move(url), std::move(body), success});
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
    if (!parse_html(html, style_blocks, stylesheet_links)) {
        return;
    }
    if (!stylesheet_links.empty()) {
        HB_LOG_INFO("[pipeline] discovered stylesheet links: " << stylesheet_links.size());
    }

    std::string css = build_css_source(style_blocks, stylesheet_links);
    parse_and_apply_css(css);

    if (!build_render_tree()) {
        return;
    }

    relayout(graphics, viewport);
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
}

bool Tab::parse_html(const std::string& html, std::vector<std::string>& style_blocks,
                     std::vector<std::string>& stylesheet_links) {
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
    return true;
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
    if (resource_provider_) {
        link_sources.reserve(stylesheet_links.size());
        for (const auto& href : stylesheet_links) {
            auto text = resource_provider_->load_text(href);
            if (!text) {
                HB_LOG_WARN("[resource] missing stylesheet: " << href);
                continue;
            }
            link_sources.push_back(std::move(*text));
        }
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
