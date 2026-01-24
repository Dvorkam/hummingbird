#include "engine/DocumentPipeline.h"

#include <algorithm>
#include <ostream>
#include <utility>

#include "core/dom/Element.h"
#include "core/dom/Node.h"
#include "core/platform_api/IGraphicsContext.h"
#include "core/platform_api/IResourceProvider.h"
#include "core/utils/Log.h"
#include "core/utils/Timing.h"
#include "core/utils/Utf8Utils.h"
#include "engine/ResourceStore.h"
#include "engine/ResourceUrl.h"
#include "html/HtmlAttributeNames.h"
#include "html/HtmlParser.h"
#include "html/HtmlTagNames.h"
#include "layout/GeometryUtils.h"
#include "layout/LayoutMetricsUtils.h"
#include "layout/RenderImage.h"
#include "layout/RenderObject.h"
#include "layout/RenderTreeTraversal.h"
#include "layout/TextStyleUtils.h"
#include "style/CssParser.h"
#include "style/Stylesheet.h"
#include "style/StylesheetSource.h"

namespace Hummingbird {
struct ImageBitmap;
}  // namespace Hummingbird

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
    const DOM::Node* current = node;
    while (current) {
        auto* element = dynamic_cast<const DOM::Element*>(current);
        if (element && element->get_tag_name() == Hummingbird::Html::TagNames::A) {
            const auto* href = element->find_attribute(Hummingbird::Html::AttributeNames::Href);
            if (!href || href->empty()) {
                return std::nullopt;
            }
            auto resolved = resolve_resource_url(base_url, *href);
            return resolved.resolved.empty() ? std::optional<std::string>(*href)
                                             : std::optional<std::string>(std::move(resolved.resolved));
        }
        current = current->get_parent();
    }
    return std::nullopt;
}

bool is_input_element(const DOM::Element* element) {
    return element && element->get_tag_name() == Hummingbird::Html::TagNames::Input;
}

std::string input_value(const DOM::Element& element) {
    if (const auto* value = element.find_attribute(Hummingbird::Html::AttributeNames::Value)) {
        return *value;
    }
    return {};
}

void set_input_value(DOM::Element& element, std::string_view value) {
    element.set_attribute(Hummingbird::Html::AttributeNames::Value, value);
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
    focused_input_ = nullptr;
    input_caret_ = 0;
}

bool DocumentPipeline::parse_html(std::string_view html) {
    const auto parse_start = Core::Clock::now();
    Html::Parser::Result parse_result;
    Html::Parser parser(dom_arena_, html);
    parse_result = parser.parse();
    const auto parse_end = Core::Clock::now();

    dom_tree_ = std::move(parse_result.dom);
    style_blocks_ = std::move(parse_result.style_blocks);
    stylesheet_links_ = std::move(parse_result.stylesheet_links);
    image_links_ = std::move(parse_result.image_links);

    if (!dom_tree_) {
        if (dom_arena_.failed()) {
            HB_LOG_ERROR("[pipeline] DOM arena budget exceeded, resetting document");
            reset();
        }
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
    Layout::Point offset{0.0f, 0.0f};
    Layout::Traversal::traverse_render_tree(
        *render_tree_, offset,
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
    paint_input_controls(graphics, context);
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
    std::optional<std::string> result;

    Layout::Traversal::traverse_render_tree(
        *render_tree_, offset,
        [&](const Layout::RenderObject& node, const Layout::Rect& absolute, const Layout::Point& /*local_offset*/) {
            if (!Layout::rect_intersects(absolute, context.viewport) ||
                !Layout::rect_contains_point(absolute, context.point)) {
                return Layout::Traversal::TraverseAction::SkipChildren;
            }
            return Layout::Traversal::TraverseAction::Continue;
        },
        [&](const Layout::RenderObject& node, const Layout::Rect& /*absolute*/, const Layout::Point& /*local_offset*/) {
            auto hit = resolve_anchor_href(node.get_dom_node(), context.base_url);
            if (hit) {
                result = std::move(*hit);
                return Layout::Traversal::TraverseAction::Stop;
            }
            return Layout::Traversal::TraverseAction::Continue;
        },
        Layout::Traversal::ChildOrder::Reverse);

    return result;
}

bool DocumentPipeline::focus_input_at(const HitTestContext& context) {
    DOM::Element* hit = hit_test_input(context);
    if (hit == focused_input_) {
        input_caret_ = focused_input_ ? input_value(*focused_input_).size() : 0;
        return focused_input_ != nullptr;
    }
    focused_input_ = hit;
    input_caret_ = focused_input_ ? input_value(*focused_input_).size() : 0;
    return focused_input_ != nullptr;
}

bool DocumentPipeline::clear_input_focus() {
    if (!focused_input_) return false;
    focused_input_ = nullptr;
    input_caret_ = 0;
    return true;
}

DocumentPipeline::InputEditResult DocumentPipeline::handle_text_input(std::string_view text) {
    InputEditResult result;
    if (!focused_input_ || text.empty()) return result;

    std::string value = input_value(*focused_input_);
    input_caret_ = Core::Utils::clamp_caret(input_caret_, value);
    value.insert(input_caret_, text);
    input_caret_ += text.size();
    set_input_value(*focused_input_, value);

    result.handled = true;
    result.needs_repaint = true;
    return result;
}

DocumentPipeline::InputEditResult DocumentPipeline::handle_key_down(const InputEvent& event) {
    InputEditResult result;
    if (!focused_input_) return result;

    std::string value = input_value(*focused_input_);

    if (event.key.key == Key::Backspace) {
        result.handled = true;
        if (!value.empty()) {
            input_caret_ = Core::Utils::clamp_caret(input_caret_, value);
            if (input_caret_ > 0) {
                auto start = Core::Utils::prev_codepoint(value, input_caret_);
                value.erase(start, input_caret_ - start);
                input_caret_ = start;
                set_input_value(*focused_input_, value);
            }
        }
        result.needs_repaint = true;
        return result;
    }

    if (event.key.key == Key::Delete) {
        result.handled = true;
        if (!value.empty()) {
            input_caret_ = Core::Utils::clamp_caret(input_caret_, value);
            if (input_caret_ < value.size()) {
                auto end = Core::Utils::next_codepoint(value, input_caret_);
                value.erase(input_caret_, end - input_caret_);
                set_input_value(*focused_input_, value);
            }
        }
        result.needs_repaint = true;
        return result;
    }

    if (event.key.key == Key::Left) {
        input_caret_ = Core::Utils::prev_codepoint(value, input_caret_);
        result.handled = true;
        result.needs_repaint = true;
        return result;
    }

    if (event.key.key == Key::Right) {
        input_caret_ = Core::Utils::next_codepoint(value, input_caret_);
        result.handled = true;
        result.needs_repaint = true;
        return result;
    }

    if (event.key.key == Key::Home) {
        input_caret_ = 0;
        result.handled = true;
        result.needs_repaint = true;
        return result;
    }

    if (event.key.key == Key::End) {
        input_caret_ = value.size();
        result.handled = true;
        result.needs_repaint = true;
        return result;
    }

    return result;
}

std::optional<std::string> DocumentPipeline::focused_input_value() const {
    if (!focused_input_) return std::nullopt;
    return input_value(*focused_input_);
}

DOM::Element* DocumentPipeline::hit_test_input(const HitTestContext& context) const {
    if (!render_tree_) {
        return nullptr;
    }
    if (!Layout::rect_contains_point(context.viewport, context.point)) {
        return nullptr;
    }

    Layout::Point offset{0.0f, -context.scroll_y};
    DOM::Element* result = nullptr;

    Layout::Traversal::traverse_render_tree(
        *render_tree_, offset,
        [&](const Layout::RenderObject& /*node*/, const Layout::Rect& absolute, const Layout::Point& /*local_offset*/) {
            if (!Layout::rect_intersects(absolute, context.viewport) ||
                !Layout::rect_contains_point(absolute, context.point)) {
                return Layout::Traversal::TraverseAction::SkipChildren;
            }
            return Layout::Traversal::TraverseAction::Continue;
        },
        [&](const Layout::RenderObject& node, const Layout::Rect& /*absolute*/, const Layout::Point& /*local_offset*/) {
            auto* element = dynamic_cast<const DOM::Element*>(node.get_dom_node());
            if (!is_input_element(element)) {
                return Layout::Traversal::TraverseAction::Continue;
            }
            result = const_cast<DOM::Element*>(element);
            return Layout::Traversal::TraverseAction::Stop;
        },
        Layout::Traversal::ChildOrder::Reverse);

    return result;
}

void DocumentPipeline::paint_input_controls(IGraphicsContext& graphics, const PaintContext& context) const {
    if (!render_tree_) return;

    Layout::Point offset{0.0f, -context.scroll_y};
    Layout::Traversal::traverse_render_tree(
        *render_tree_, offset,
        [&](const Layout::RenderObject& node, const Layout::Rect& absolute, const Layout::Point& /*local_offset*/) {
            if (context.viewport.width > 0.0f && context.viewport.height > 0.0f &&
                !Layout::rect_intersects(absolute, context.viewport)) {
                return Layout::Traversal::TraverseAction::SkipChildren;
            }

            auto* element = dynamic_cast<const DOM::Element*>(node.get_dom_node());
            if (!is_input_element(element)) {
                return Layout::Traversal::TraverseAction::Continue;
            }

            const auto* style = node.get_computed_style();
            Layout::Metrics::Insets insets = Layout::Metrics::compute_insets(style);
            Layout::Rect content = {absolute.x + insets.left, absolute.y + insets.top,
                                    absolute.width - insets.left - insets.right,
                                    absolute.height - insets.top - insets.bottom};
            if (content.width <= 0.0f || content.height <= 0.0f) {
                return Layout::Traversal::TraverseAction::Continue;
            }

            TextStyle text_style = Layout::TextStyleUtils::build_text_style(style);
            std::string value = input_value(*element);
            TextMetrics metrics = graphics.measure_text(value, text_style);
            TextMetrics caret_metrics = metrics.height > 0.0f ? metrics : graphics.measure_text("A", text_style);
            float text_height = metrics.height > 0.0f ? metrics.height : caret_metrics.height;
            float text_x = content.x;
            float text_y = content.y + std::max(0.0f, (content.height - text_height) * 0.5f);

            if (!value.empty()) {
                graphics.draw_text(value, text_x, text_y, text_style);
            }

            if (element == focused_input_) {
                auto caret = Core::Utils::clamp_caret(input_caret_, value);
                std::string prefix = value.substr(0, caret);
                float caret_offset = graphics.measure_text(prefix, text_style).width;
                float caret_x = text_x + caret_offset;
                float max_caret_x = content.x + std::max(0.0f, content.width - 1.0f);
                if (caret_x > max_caret_x) {
                    caret_x = max_caret_x;
                }
                Layout::Rect caret_rect{caret_x, text_y, 1.0f, text_height};
                graphics.fill_rect(caret_rect, text_style.color);
            }

            return Layout::Traversal::TraverseAction::Continue;
        });
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
