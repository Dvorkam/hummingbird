#include "engine/document/DocumentPipeline.h"

#include <ostream>
#include <utility>

#include "core/platform_api/IGraphicsContext.h"
#include "core/platform_api/InputEvent.h"
#include "core/utils/Log.h"
#include "core/utils/Timing.h"
#include "layout/RenderObject.h"
#include "layout/geometry/GeometryUtils.h"
#include "layout/geometry/PositioningUtils.h"

namespace Hummingbird {
struct ImageBitmap;
}  // namespace Hummingbird

namespace Hummingbird::Engine {

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
    return script_controller_.run_inline_scripts(model_.script_blocks(), model_.dom_root(), model_.dom_arena());
}

void DocumentPipeline::set_extension_style_blocks(const std::vector<std::string>& style_blocks) {
    extension_style_blocks_ = style_blocks;
}

DocumentPipeline::ScriptDispatchResult DocumentPipeline::dispatch_click(const HitTestContext& context) {
    return script_controller_.dispatch_click(model_.dom_root(), model_.dom_arena(), model_.render_tree(),
                                             context.viewport, context.point, context.scroll_y);
}

DocumentPipeline::ScriptDispatchResult DocumentPipeline::dispatch_load() {
    return script_controller_.dispatch_load(model_.dom_root(), model_.dom_arena());
}

void DocumentPipeline::apply_styles_and_layout(IGraphicsContext& graphics, const Layout::Rect& viewport,
                                               std::string_view base_url) {
    std::vector<std::string> style_blocks = model_.style_blocks();
    std::string css =
        resources_.build_css_source(base_url, style_blocks, model_.stylesheet_links(), extension_style_blocks_);
    model_.apply_styles(css);

    if (!model_.build_render_tree()) {
        return;
    }

    update_image_resources(base_url);
    relayout(graphics, viewport);
}

bool DocumentPipeline::rebuild_and_layout(IGraphicsContext& graphics, const Layout::Rect& viewport,
                                          std::string_view base_url) {
    apply_styles_and_layout(graphics, viewport, base_url);
    return model_.has_render_tree();
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
    return navigation_.hit_test_link(model_.render_tree(), context.point, context.viewport, context.scroll_y,
                                     context.base_url);
}

std::optional<FormSubmission> DocumentPipeline::submit_form_at(const HitTestContext& context) const {
    return navigation_.submit_form_at(model_.render_tree(), context.point, context.viewport, context.scroll_y,
                                      context.base_url);
}

bool DocumentPipeline::focus_input_at(const HitTestContext& context) {
    return input_controller_.focus_input_at(model_.render_tree(), context.point, context.viewport, context.scroll_y);
}

bool DocumentPipeline::focus_autofocus_input() {
    return input_controller_.focus_autofocus_input(model_.render_tree());
}

bool DocumentPipeline::clear_input_focus() {
    return input_controller_.clear_focus();
}

bool DocumentPipeline::set_control_interaction_at(const HitTestContext& context) {
    return input_controller_.set_control_interaction_at(model_.render_tree(), context.point, context.viewport,
                                                        context.scroll_y);
}

bool DocumentPipeline::clear_control_interaction() {
    return input_controller_.clear_control_interaction();
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
            output.submitted_form = model_.build_form_submission(*focused, base_url);
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
