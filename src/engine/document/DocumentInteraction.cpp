#include "engine/document/DocumentInteraction.h"

#include "engine/document/DocumentModel.h"

namespace Hummingbird::Engine {

DocumentInteraction::DocumentInteraction(DocumentModel& model) : model_(model), navigation_(model) {}

void DocumentInteraction::reset() {
    input_controller_.reset();
}

std::optional<std::string> DocumentInteraction::hit_test_link(const HitTestContext& context) const {
    return navigation_.hit_test_link(model_.render_tree(), context.point, context.viewport, context.scroll_y,
                                     context.base_url);
}

std::optional<FormSubmission> DocumentInteraction::submit_form_at(const HitTestContext& context) const {
    return navigation_.submit_form_at(model_.render_tree(), context.point, context.viewport, context.scroll_y,
                                      context.base_url);
}

bool DocumentInteraction::focus_input_at(const Layout::RenderObject* render_tree, const HitTestContext& context) {
    return input_controller_.focus_input_at(render_tree, context.point, context.viewport, context.scroll_y);
}

bool DocumentInteraction::focus_autofocus_input(const Layout::RenderObject* render_tree) {
    return input_controller_.focus_autofocus_input(render_tree);
}

bool DocumentInteraction::clear_input_focus() {
    return input_controller_.clear_focus();
}

bool DocumentInteraction::set_control_interaction_at(const Layout::RenderObject* render_tree,
                                                     const HitTestContext& context) {
    return input_controller_.set_control_interaction_at(render_tree, context.point, context.viewport, context.scroll_y);
}

bool DocumentInteraction::clear_control_interaction() {
    return input_controller_.clear_control_interaction();
}

DocumentInteraction::InputEditResult DocumentInteraction::handle_text_input(std::string_view text) {
    auto result = input_controller_.handle_text_input(text);
    return {result.handled, result.needs_repaint, std::nullopt};
}

DocumentInteraction::InputEditResult DocumentInteraction::handle_key_down(const InputEvent& event,
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

void DocumentInteraction::paint_controls(const Layout::RenderObject* render_tree, IGraphicsContext& graphics,
                                         const Layout::Rect& viewport, float scroll_y, bool repaint_background) const {
    input_controller_.paint_controls(render_tree, graphics, viewport, scroll_y, repaint_background);
}

}  // namespace Hummingbird::Engine
