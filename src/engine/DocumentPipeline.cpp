#include "engine/DocumentPipeline.h"

#include <algorithm>

#include "core/dom/Element.h"
#include "core/dom/Node.h"
#include "core/platform_api/IResourceProvider.h"
#include "core/utils/Log.h"
#include "core/utils/Timing.h"
#include "engine/ResourceStore.h"
#include "engine/ResourceUrl.h"
#include "html/HtmlAttributeNames.h"
#include "html/HtmlParser.h"
#include "html/HtmlTagNames.h"
#include "layout/GeometryUtils.h"
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
    if (!Layout::rect_intersects(absolute, viewport)) {
        return std::nullopt;
    }
    if (!Layout::rect_contains_point(absolute, point)) {
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

DocumentPipeline::DocumentPipeline(ResourceStore* resource_store, IResourceProvider* resource_provider)
    : resource_store_(resource_store), resource_provider_(resource_provider) {}

DocumentPipeline::~DocumentPipeline() = default;

void DocumentPipeline::reset() {
    dom_tree_.reset();
    render_tree_.reset();
    dom_arena_.reset();
    style_blocks_.clear();
    stylesheet_links_.clear();
    image_links_.clear();
    content_height_ = 0.0f;
}

bool DocumentPipeline::parse_html(std::string_view html) {
    const auto parse_start = Core::Clock::now();
    Html::Parser parser(dom_arena_, html);
    auto parse_result = parser.parse();
    const auto parse_end = Core::Clock::now();

    dom_tree_ = std::move(parse_result.dom);
    style_blocks_ = std::move(parse_result.style_blocks);
    stylesheet_links_ = std::move(parse_result.stylesheet_links);
    image_links_ = std::move(parse_result.image_links);

    if (!dom_tree_) {
        HB_LOG_WARN("[pipeline] parsed empty DOM");
        return false;
    }

    HB_LOG_INFO("[pipeline] parsed DOM children: " << dom_tree_->get_children().size()
                                                   << " total nodes: " << count_nodes_recursive(dom_tree_.get()));
    HB_LOG_INFO("[perf] html parse ms=" << Core::duration_ms(parse_start, parse_end));

    return true;
}

void DocumentPipeline::apply_styles_and_layout(IGraphicsContext& graphics, const Layout::Rect& viewport,
                                               std::string_view base_url) {
    std::string css = build_css_source(base_url);
    parse_and_apply_css(css);

    if (!build_render_tree()) {
        return;
    }

    update_image_resources(base_url);
    relayout(graphics, viewport);
}

bool DocumentPipeline::update_image_resources(std::string_view base_url) {
    if (!render_tree_ || !resource_store_) {
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
                auto resolved = resolve_resource_url(base_url, it->second);
                std::string_view key = resolved.key;
                auto view = resource_store_->view(key, ResourceType::Image);
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

void DocumentPipeline::relayout(IGraphicsContext& graphics, const Layout::Rect& viewport) {
    if (!render_tree_) return;

    const auto layout_start = Core::Clock::now();
    render_tree_->layout(graphics, viewport);
    const auto layout_end = Core::Clock::now();
    content_height_ = render_tree_->get_rect().height;
    HB_LOG_INFO("[perf] layout ms=" << Core::duration_ms(layout_start, layout_end) << " viewport=" << viewport.width
                                    << "x" << viewport.height);
}

void DocumentPipeline::paint(IGraphicsContext& graphics, const PaintContext& context) {
    if (!render_tree_) return;

    graphics.set_viewport(context.viewport);

    Renderer::PaintOptions opts;
    opts.debug_outlines = context.debug_outlines;
    opts.scroll_y = context.scroll_y;
    opts.viewport = context.viewport;

    const auto paint_start = Core::Clock::now();
    painter_.paint(*render_tree_, graphics, opts);
    const auto paint_end = Core::Clock::now();
    static int paint_log_counter = 0;
    if (++paint_log_counter % 5 == 0) {
        HB_LOG_DEBUG("[perf] paint ms=" << Core::duration_ms(paint_start, paint_end)
                                        << " scroll_y=" << context.scroll_y);
    }
}

std::optional<std::string> DocumentPipeline::hit_test_link(const HitTestContext& context) const {
    if (!render_tree_) {
        return std::nullopt;
    }
    if (!Layout::rect_contains_point(context.viewport, context.point)) {
        return std::nullopt;
    }
    Layout::Point offset{0.0f, -context.scroll_y};
    return hit_test_link_recursive(*render_tree_, offset, context.point, context.viewport, context.base_url);
}

size_t DocumentPipeline::render_tree_children() const {
    if (!render_tree_) return 0;
    return render_tree_->get_children().size();
}

std::string DocumentPipeline::build_css_source(std::string_view base_url) const {
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
        return Css::merge_css_sources(ua_css, {}, style_blocks_);
    }

    std::vector<std::string> link_sources;
    link_sources.reserve(stylesheet_links_.size());
    size_t ready_count = 0;
    size_t loading_count = 0;
    size_t missing_count = 0;
    size_t failed_count = 0;
    for (const auto& href : stylesheet_links_) {
        auto resolved = resolve_resource_url(base_url, href);
        std::string_view key = resolved.key;
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
    if (!stylesheet_links_.empty()) {
        HB_LOG_DEBUG("[style] link stylesheets ready=" << ready_count << " loading=" << loading_count
                                                       << " failed=" << failed_count << " missing=" << missing_count);
    }

    return Css::merge_css_sources(ua_css, link_sources, style_blocks_);
}

void DocumentPipeline::parse_and_apply_css(const std::string& css) {
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

bool DocumentPipeline::build_render_tree() {
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

}  // namespace Hummingbird::Engine
