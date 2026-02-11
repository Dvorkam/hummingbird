#include "engine/document/DocumentPipeline.h"

#include <cstdlib>
#include <ostream>
#include <utility>

#include "core/platform_api/IGraphicsContext.h"
#include "core/platform_api/InputEvent.h"
#include "core/utils/Log.h"
#include "engine/script/DocumentScriptController.h"

namespace Hummingbird::Engine {

namespace {
bool relayout_debug_enabled() {
    static const bool enabled = std::getenv("HB_DEBUG_LAYOUT_STATE") != nullptr;
    return enabled;
}
}  // namespace

DocumentPipeline::DocumentPipeline(ResourceStore* resource_store, IResourceProvider* resource_provider,
                                   IImageDecoder* image_decoder, ScriptEnginePtr script_engine)
    : resources_(resource_store, resource_provider, image_decoder),
      script_controller_(std::make_unique<DocumentScriptController>(std::move(script_engine))) {}

DocumentPipeline::~DocumentPipeline() = default;

void DocumentPipeline::reset() {
    model_.reset();
    interaction_.reset();
    script_controller_->clear();
    renderer_.reset();
}

bool DocumentPipeline::parse_html(std::string_view html) {
    auto result = model_.parse_html(html);
    if (!result.ok && result.arena_failed) {
        reset();
    }
    return result.ok;
}

bool DocumentPipeline::run_scripts() {
    return script_controller_->run_inline_scripts(model_.script_blocks(), model_.dom_root(), model_.dom_arena());
}

void DocumentPipeline::set_extension_style_blocks(const std::vector<std::string>& style_blocks) {
    style_coordinator_.set_extension_style_blocks(style_blocks);
}

DocumentPipeline::ScriptDispatchResult DocumentPipeline::dispatch_click(const HitTestContext& context) {
    auto result = script_controller_->dispatch_click(model_.dom_root(), model_.dom_arena(), model_.render_tree(),
                                                     context.viewport, context.point, context.scroll_y);
    return {result.handled, result.mutated};
}

DocumentPipeline::ScriptDispatchResult DocumentPipeline::dispatch_load() {
    auto result = script_controller_->dispatch_load(model_.dom_root(), model_.dom_arena());
    return {result.handled, result.mutated};
}

void DocumentPipeline::apply_styles_and_layout(IGraphicsContext& graphics, const Layout::Rect& viewport,
                                               std::string_view base_url) {
    if (!style_coordinator_.apply_styles_and_build(base_url)) {
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
    return style_coordinator_.update_image_resources(base_url);
}

void DocumentPipeline::relayout(IGraphicsContext& graphics, const Layout::Rect& viewport) {
    renderer_.relayout(graphics, viewport);
    if (relayout_debug_enabled()) {
        HB_LOG_WARN("[layout-debug] relayout content_h=" << renderer_.content_height());
    }
}

void DocumentPipeline::paint(IGraphicsContext& graphics, const PaintContext& context) {
    renderer_.paint(graphics, context);
}

void DocumentPipeline::paint_controls(IGraphicsContext& graphics, const PaintContext& context,
                                      bool repaint_background) {
    renderer_.paint_controls(graphics, context, repaint_background);
}

std::optional<std::string> DocumentPipeline::hit_test_link(const HitTestContext& context) const {
    return interaction_.hit_test_link({context.point, context.viewport, context.base_url, context.scroll_y});
}

std::optional<FormSubmission> DocumentPipeline::submit_form_at(const HitTestContext& context) const {
    return interaction_.submit_form_at({context.point, context.viewport, context.base_url, context.scroll_y});
}

bool DocumentPipeline::focus_input_at(const HitTestContext& context) {
    return interaction_.focus_input_at(model_.render_tree(),
                                       {context.point, context.viewport, context.base_url, context.scroll_y});
}

bool DocumentPipeline::focus_autofocus_input() {
    return interaction_.focus_autofocus_input(model_.render_tree());
}

bool DocumentPipeline::clear_input_focus() {
    return interaction_.clear_input_focus();
}

bool DocumentPipeline::set_control_interaction_at(const HitTestContext& context) {
    return interaction_.set_control_interaction_at(
        model_.render_tree(), {context.point, context.viewport, context.base_url, context.scroll_y});
}

bool DocumentPipeline::clear_control_interaction() {
    return interaction_.clear_control_interaction();
}

DocumentPipeline::InputEditResult DocumentPipeline::handle_text_input(std::string_view text) {
    return interaction_.handle_text_input(text);
}

DocumentPipeline::InputEditResult DocumentPipeline::handle_key_down(const InputEvent& event,
                                                                    std::string_view base_url) {
    return interaction_.handle_key_down(event, base_url);
}

std::optional<std::string> DocumentPipeline::focused_input_value() const {
    return interaction_.focused_input_value();
}

size_t DocumentPipeline::render_tree_children() const {
    return model_.render_tree_children();
}

}  // namespace Hummingbird::Engine
