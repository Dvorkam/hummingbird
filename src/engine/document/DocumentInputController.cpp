#include "engine/document/DocumentInputController.h"

#include "core/dom/Element.h"
#include "core/utils/Log.h"
#include "core/utils/TextEditBuffer.h"
#include "engine/document/DocumentInputPainter.h"
#include "engine/document/DocumentInputUtils.h"
#include "engine/document/HitTestUtils.h"
#include "layout/RenderObject.h"
#include "layout/geometry/GeometryUtils.h"
#include "layout/paint/RenderTreeTraversal.h"

namespace Hummingbird::Engine {

namespace {
DOM::Element* hit_test_input(const Layout::RenderObject* render_tree, const Layout::Point& point,
                             const Layout::Rect& viewport, float scroll_y) {
    auto hit = HitTest::hit_test_z_order<DOM::Element*>(
        render_tree, point, viewport, scroll_y, [&](const Layout::RenderObject& node) -> std::optional<DOM::Element*> {
            auto* element = dynamic_cast<const DOM::Element*>(node.get_dom_node());
            if (!is_editable_input_element(element)) {
                return std::nullopt;
            }
            return const_cast<DOM::Element*>(element);
        });
    return hit.value_or(nullptr);
}

DOM::Element* hit_test_interactive_control(const Layout::RenderObject* render_tree, const Layout::Point& point,
                                           const Layout::Rect& viewport, float scroll_y) {
    auto hit = HitTest::hit_test_z_order<DOM::Element*>(
        render_tree, point, viewport, scroll_y, [&](const Layout::RenderObject& node) -> std::optional<DOM::Element*> {
            auto* element = dynamic_cast<const DOM::Element*>(node.get_dom_node());
            if (!is_interactive_control_element(element)) {
                return std::nullopt;
            }
            return const_cast<DOM::Element*>(element);
        });
    return hit.value_or(nullptr);
}
}  // namespace

bool DocumentInputController::set_control_interaction_at(const Layout::RenderObject* render_tree,
                                                         const Layout::Point& point, const Layout::Rect& viewport,
                                                         float scroll_y) {
    DOM::Element* hit = hit_test_interactive_control(render_tree, point, viewport, scroll_y);
    bool changed = false;
    if (hovered_active_control_ && hovered_active_control_ != hit) {
        changed |= hovered_active_control_->set_pseudo_state(DOM::Element::PseudoState::Hover, false);
        changed |= hovered_active_control_->set_pseudo_state(DOM::Element::PseudoState::Active, false);
    }
    hovered_active_control_ = hit;
    if (hovered_active_control_) {
        changed |= hovered_active_control_->set_pseudo_state(DOM::Element::PseudoState::Hover, true);
        changed |= hovered_active_control_->set_pseudo_state(DOM::Element::PseudoState::Active, true);
    }
    return changed;
}

bool DocumentInputController::clear_control_interaction() {
    if (!hovered_active_control_) {
        return false;
    }
    bool changed = false;
    changed |= hovered_active_control_->set_pseudo_state(DOM::Element::PseudoState::Hover, false);
    changed |= hovered_active_control_->set_pseudo_state(DOM::Element::PseudoState::Active, false);
    hovered_active_control_ = nullptr;
    return changed;
}

void DocumentInputController::reset() {
    clear_control_interaction();
    clear_focus();
}

bool DocumentInputController::focus_input_at(const Layout::RenderObject* render_tree, const Layout::Point& point,
                                             const Layout::Rect& viewport, float scroll_y) {
    DOM::Element* hit = hit_test_input(render_tree, point, viewport, scroll_y);
    HB_LOG_DEBUG("[input] focus_input_at point=(" << point.x << "," << point.y << ") scroll_y=" << scroll_y
                                                  << " hit=" << describe_input_target(hit));
    if (hit == focused_input_) {
        caret_ = focused_input_ ? input_value(*focused_input_).size() : 0;
        return focused_input_ != nullptr;
    }
    if (focused_input_) {
        focused_input_->set_pseudo_state(DOM::Element::PseudoState::Focus, false);
    }
    focused_input_ = hit;
    if (focused_input_) {
        focused_input_->set_pseudo_state(DOM::Element::PseudoState::Focus, true);
    }
    caret_ = focused_input_ ? input_value(*focused_input_).size() : 0;
    return focused_input_ != nullptr;
}

bool DocumentInputController::focus_autofocus_input(const Layout::RenderObject* render_tree) {
    if (!render_tree || focused_input_) {
        return false;
    }

    DOM::Element* autofocus_element = nullptr;
    Layout::Point offset{0.0f, 0.0f};
    Layout::Traversal::traverse_render_tree(
        *render_tree, offset,
        [&](const Layout::RenderObject& node, const Layout::Rect& /*absolute*/, const Layout::Point& /*local_offset*/) {
            auto* element = dynamic_cast<const DOM::Element*>(node.get_dom_node());
            if (!is_autofocus_input_element(element)) {
                return Layout::Traversal::TraverseAction::Continue;
            }
            autofocus_element = const_cast<DOM::Element*>(element);
            return Layout::Traversal::TraverseAction::Stop;
        });

    if (!autofocus_element) {
        return false;
    }
    focused_input_ = autofocus_element;
    focused_input_->set_pseudo_state(DOM::Element::PseudoState::Focus, true);
    caret_ = input_value(*focused_input_).size();
    return true;
}

bool DocumentInputController::clear_focus() {
    if (!focused_input_) {
        return false;
    }
    focused_input_->set_pseudo_state(DOM::Element::PseudoState::Focus, false);
    focused_input_ = nullptr;
    caret_ = 0;
    return true;
}

DocumentInputController::EditResult DocumentInputController::handle_text_input(std::string_view text) {
    EditResult result;
    if (!focused_input_ || text.empty()) return result;

    std::string value = input_value(*focused_input_);
    if (Core::Utils::TextEditBuffer::insert_text(value, caret_, text)) {
        set_input_value(*focused_input_, value);
        HB_LOG_DEBUG("[input] text input appended bytes=" << text.size() << " new_value='" << value
                                                          << "' caret=" << caret_);
    }

    result.handled = true;
    result.needs_repaint = true;
    return result;
}

DocumentInputController::EditResult DocumentInputController::handle_key_down(const InputEvent& event) {
    EditResult result;
    if (!focused_input_) return result;

    std::string value = input_value(*focused_input_);

    if (event.key.key == Key::Backspace) {
        result.handled = true;
        if (Core::Utils::TextEditBuffer::backspace(value, caret_)) {
            set_input_value(*focused_input_, value);
        }
        result.needs_repaint = true;
        return result;
    }

    if (event.key.key == Key::Delete) {
        result.handled = true;
        if (Core::Utils::TextEditBuffer::delete_forward(value, caret_)) {
            set_input_value(*focused_input_, value);
        }
        result.needs_repaint = true;
        return result;
    }

    if (event.key.key == Key::Left) {
        Core::Utils::TextEditBuffer::move_left(value, caret_);
        result.handled = true;
        result.needs_repaint = true;
        return result;
    }

    if (event.key.key == Key::Right) {
        Core::Utils::TextEditBuffer::move_right(value, caret_);
        result.handled = true;
        result.needs_repaint = true;
        return result;
    }

    if (event.key.key == Key::Home) {
        Core::Utils::TextEditBuffer::move_home(caret_);
        result.handled = true;
        result.needs_repaint = true;
        return result;
    }

    if (event.key.key == Key::End) {
        Core::Utils::TextEditBuffer::move_end(value, caret_);
        result.handled = true;
        result.needs_repaint = true;
        return result;
    }

    return result;
}

std::optional<std::string> DocumentInputController::focused_value() const {
    if (!focused_input_) return std::nullopt;
    return input_value(*focused_input_);
}

void DocumentInputController::paint_controls(const Layout::RenderObject* render_tree, IGraphicsContext& graphics,
                                             const Layout::Rect& viewport, float scroll_y,
                                             bool repaint_background) const {
    if (!render_tree) return;

    Layout::Point offset{0.0f, -scroll_y};
    Layout::Traversal::traverse_render_tree(
        *render_tree, offset,
        [&](const Layout::RenderObject& node, const Layout::Rect& absolute, const Layout::Point& local_offset) {
            if (viewport.width > 0.0f && viewport.height > 0.0f && !Layout::rect_intersects(absolute, viewport)) {
                return Layout::Traversal::TraverseAction::SkipChildren;
            }

            auto* element = dynamic_cast<const DOM::Element*>(node.get_dom_node());
            if (!is_input_element(element)) {
                return Layout::Traversal::TraverseAction::Continue;
            }

            paint_input_control(*element, node, absolute, local_offset, graphics, repaint_background,
                                element == focused_input_, caret_, scroll_y);

            return Layout::Traversal::TraverseAction::Continue;
        });
}

}  // namespace Hummingbird::Engine
