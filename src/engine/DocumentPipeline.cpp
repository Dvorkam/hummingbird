#include "engine/DocumentPipeline.h"

#include <ostream>
#include <utility>

#include "core/dom/Element.h"
#include "core/platform_api/IGraphicsContext.h"
#include "core/utils/Log.h"
#include "core/utils/StringUtils.h"
#include "core/utils/Timing.h"
#include "engine/ResourceUrl.h"
#include "html/HtmlAttributeNames.h"
#include "html/HtmlTagNames.h"
#include "layout/GeometryUtils.h"
#include "layout/PositioningUtils.h"
#include "layout/RenderObject.h"
#include "layout/RenderTreeTraversal.h"

namespace Hummingbird {
struct ImageBitmap;
}  // namespace Hummingbird

namespace Hummingbird::Engine {

namespace {
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

const DOM::Element* resolve_submit_element(const DOM::Node* node) {
    const DOM::Node* current = node;
    while (current) {
        auto* element = dynamic_cast<const DOM::Element*>(current);
        if (element) {
            const auto& tag = element->get_tag_name();
            if (tag == Hummingbird::Html::TagNames::Button) {
                if (const auto* type = element->find_attribute(Hummingbird::Html::AttributeNames::Type)) {
                    if (!Core::Utils::equals_ignore_case(*type, "submit")) {
                        current = current->get_parent();
                        continue;
                    }
                }
                return element;
            }
            if (tag == Hummingbird::Html::TagNames::Input) {
                if (const auto* type = element->find_attribute(Hummingbird::Html::AttributeNames::Type)) {
                    if (Core::Utils::equals_ignore_case(*type, "submit")) {
                        return element;
                    }
                }
            }
        }
        current = current->get_parent();
    }
    return nullptr;
}

}  // namespace

DocumentPipeline::DocumentPipeline(ResourceStore* resource_store, IResourceProvider* resource_provider,
                                   IImageDecoder* image_decoder, ScriptEnginePtr script_engine)
    : resources_(resource_store, resource_provider, image_decoder), script_controller_(std::move(script_engine)) {}

DocumentPipeline::~DocumentPipeline() = default;

void DocumentPipeline::reset() {
    model_.reset();
    content_height_ = 0.0f;
    input_controller_.reset();
    script_controller_.clear();
    painter_.invalidate_display_list();
}

bool DocumentPipeline::parse_html(std::string_view html) {
    auto result = model_.parse_html(html);
    if (!result.ok && result.arena_failed) {
        reset();
    }
    return result.ok;
}

bool DocumentPipeline::run_scripts() {
    return script_controller_.run_inline_scripts(model_);
}

DocumentPipeline::ScriptDispatchResult DocumentPipeline::dispatch_click(const HitTestContext& context) {
    return script_controller_.dispatch_click(model_, model_.render_tree(), context.viewport, context.point,
                                             context.scroll_y);
}

DocumentPipeline::ScriptDispatchResult DocumentPipeline::dispatch_load() {
    return script_controller_.dispatch_load(model_);
}

void DocumentPipeline::apply_styles_and_layout(IGraphicsContext& graphics, const Layout::Rect& viewport,
                                               std::string_view base_url) {
    std::string css = resources_.build_css_source(base_url, model_.style_blocks(), model_.stylesheet_links());
    model_.apply_styles(css);

    if (!model_.build_render_tree()) {
        return;
    }

    update_image_resources(base_url);
    relayout(graphics, viewport);
}

bool DocumentPipeline::update_image_resources(std::string_view base_url) {
    bool updated = resources_.update_image_resources(model_.render_tree(), base_url);
    updated = resources_.update_svg_resources(model_.render_tree()) || updated;
    return updated;
}

void DocumentPipeline::relayout(IGraphicsContext& graphics, const Layout::Rect& viewport) {
    auto* render_tree = model_.render_tree();
    if (!render_tree) return;

    painter_.invalidate_display_list();

    const auto layout_start = Core::Clock::now();
    render_tree->layout(graphics, viewport);
    Layout::Positioning::apply_positioning(*render_tree, graphics, viewport);
    const auto layout_end = Core::Clock::now();
    content_height_ = render_tree->get_rect().height;
    HB_LOG_INFO("[perf] layout ms=" << Core::duration_ms(layout_start, layout_end) << " viewport=" << viewport.width
                                    << "x" << viewport.height);
}

void DocumentPipeline::paint(IGraphicsContext& graphics, const PaintContext& context) {
    if (!model_.render_tree()) return;

    const auto paint_start = Core::Clock::now();
    painter_.paint(model_.render_tree(), graphics, context.viewport, context.debug_outlines, context.scroll_y,
                   input_controller_);
    const auto paint_end = Core::Clock::now();
    static int paint_log_counter = 0;
    if (++paint_log_counter % 5 == 0) {
        HB_LOG_DEBUG("[perf] paint ms=" << Core::duration_ms(paint_start, paint_end)
                                        << " scroll_y=" << context.scroll_y);
    }
}

void DocumentPipeline::paint_controls(IGraphicsContext& graphics, const PaintContext& context,
                                      bool repaint_background) {
    auto* render_tree = model_.render_tree();
    if (!render_tree) return;

    graphics.set_viewport(context.viewport);
    input_controller_.paint_controls(render_tree, graphics, context.viewport, context.scroll_y, repaint_background);
}

std::optional<std::string> DocumentPipeline::hit_test_link(const HitTestContext& context) const {
    auto* render_tree = model_.render_tree();
    if (!render_tree) {
        return std::nullopt;
    }
    if (!Layout::rect_contains_point(context.viewport, context.point)) {
        return std::nullopt;
    }
    Layout::Point offset{0.0f, -context.scroll_y};
    std::optional<std::string> result;

    Layout::Positioning::traverse_render_tree_z_order(
        *render_tree, offset,
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

std::optional<std::string> DocumentPipeline::submit_form_at(const HitTestContext& context) const {
    auto* render_tree = model_.render_tree();
    if (!render_tree) {
        return std::nullopt;
    }
    if (!Layout::rect_contains_point(context.viewport, context.point)) {
        return std::nullopt;
    }

    Layout::Point offset{0.0f, -context.scroll_y};
    std::optional<std::string> result;

    Layout::Positioning::traverse_render_tree_z_order(
        *render_tree, offset,
        [&](const Layout::RenderObject& /*node*/, const Layout::Rect& absolute, const Layout::Point& /*local_offset*/) {
            if (!Layout::rect_intersects(absolute, context.viewport) ||
                !Layout::rect_contains_point(absolute, context.point)) {
                return Layout::Traversal::TraverseAction::SkipChildren;
            }
            return Layout::Traversal::TraverseAction::Continue;
        },
        [&](const Layout::RenderObject& node, const Layout::Rect& /*absolute*/, const Layout::Point& /*local_offset*/) {
            const auto* submit = resolve_submit_element(node.get_dom_node());
            if (!submit) {
                return Layout::Traversal::TraverseAction::Continue;
            }
            auto url = model_.build_form_submission_url(*submit, context.base_url);
            if (url) {
                result = std::move(*url);
                return Layout::Traversal::TraverseAction::Stop;
            }
            return Layout::Traversal::TraverseAction::Continue;
        },
        Layout::Traversal::ChildOrder::Reverse);

    return result;
}

bool DocumentPipeline::focus_input_at(const HitTestContext& context) {
    return input_controller_.focus_input_at(model_.render_tree(), context.point, context.viewport, context.scroll_y);
}

bool DocumentPipeline::clear_input_focus() {
    return input_controller_.clear_focus();
}

DocumentPipeline::InputEditResult DocumentPipeline::handle_text_input(std::string_view text) {
    auto result = input_controller_.handle_text_input(text);
    return {result.handled, result.needs_repaint, std::nullopt};
}

DocumentPipeline::InputEditResult DocumentPipeline::handle_key_down(const InputEvent& event,
                                                                    std::string_view base_url) {
    auto result = input_controller_.handle_key_down(event);
    InputEditResult output{result.handled, result.needs_repaint, std::nullopt};

    if (event.key.key == Key::Enter && input_controller_.has_focus()) {
        const auto* focused = input_controller_.focused_element();
        if (focused) {
            output.submitted_url = model_.build_form_submission_url(*focused, base_url);
        }
        output.handled = true;
    }

    return output;
}

std::optional<std::string> DocumentPipeline::focused_input_value() const {
    return input_controller_.focused_value();
}

size_t DocumentPipeline::render_tree_children() const {
    return model_.render_tree_children();
}

}  // namespace Hummingbird::Engine
