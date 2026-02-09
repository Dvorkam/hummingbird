#include "engine/document/DocumentInputController.h"

#include <algorithm>
#include <cstdlib>

#include "core/dom/Element.h"
#include "core/utils/Log.h"
#include "core/utils/StringUtils.h"
#include "core/utils/TextEditBuffer.h"
#include "engine/document/HitTestUtils.h"
#include "html/HtmlAttributeNames.h"
#include "html/HtmlTagNames.h"
#include "layout/RenderObject.h"
#include "layout/flow/TextStyleUtils.h"
#include "layout/geometry/GeometryUtils.h"
#include "layout/geometry/metrics/LayoutMetricsUtils.h"
#include "layout/paint/RenderTreeTraversal.h"

namespace Hummingbird::Engine {

namespace {
constexpr float kCaretWidth = 1.0f;
constexpr const char* kCaretFallbackGlyph = "A";
constexpr Color kFocusRingColor{66, 133, 244, 255};

struct InputPaintData {
    Layout::Rect absolute;
    Layout::Rect content;
    TextStyle text_style;
    std::string value;
    float text_x;
    float text_y;
    float text_height;
};

bool is_input_element(const DOM::Element* element) {
    return element && element->get_tag_name() == Hummingbird::Html::TagNames::Input;
}

bool is_button_element(const DOM::Element* element) {
    return element && element->get_tag_name() == Hummingbird::Html::TagNames::Button;
}

bool is_interactive_control_element(const DOM::Element* element) {
    return is_input_element(element) || is_button_element(element);
}

Color high_contrast_text(Color background) {
    const int luma = (static_cast<int>(background.r) * 299 + static_cast<int>(background.g) * 587 +
                      static_cast<int>(background.b) * 114) /
                     1000;
    return luma >= 128 ? Color{0, 0, 0, 255} : Color{255, 255, 255, 255};
}

bool low_contrast(Color foreground, Color background) {
    const int dr = std::abs(static_cast<int>(foreground.r) - static_cast<int>(background.r));
    const int dg = std::abs(static_cast<int>(foreground.g) - static_cast<int>(background.g));
    const int db = std::abs(static_cast<int>(foreground.b) - static_cast<int>(background.b));
    return (dr + dg + db) < 48;
}

bool is_editable_input_element(const DOM::Element* element) {
    if (!is_input_element(element)) {
        return false;
    }

    const auto* type = element->find_attribute(Hummingbird::Html::AttributeNames::Type);
    if (!type || type->empty()) {
        return true;  // default <input> type is text
    }

    return !Core::Utils::equals_ignore_case(*type, "button") && !Core::Utils::equals_ignore_case(*type, "submit") &&
           !Core::Utils::equals_ignore_case(*type, "reset") && !Core::Utils::equals_ignore_case(*type, "checkbox") &&
           !Core::Utils::equals_ignore_case(*type, "radio") && !Core::Utils::equals_ignore_case(*type, "file") &&
           !Core::Utils::equals_ignore_case(*type, "hidden") && !Core::Utils::equals_ignore_case(*type, "image");
}

bool is_autofocus_input_element(const DOM::Element* element) {
    if (!is_editable_input_element(element)) {
        return false;
    }
    return element->find_attribute(Hummingbird::Html::AttributeNames::Autofocus) != nullptr;
}

std::string input_value(const DOM::Element& element) {
    if (const auto* value = element.find_attribute(Hummingbird::Html::AttributeNames::Value)) {
        return *value;
    }
    return {};
}

std::string describe_input_target(const DOM::Element* element) {
    if (!element) {
        return "<none>";
    }
    std::string desc = "<" + element->get_tag_name() + ">";
    if (const auto* id = element->find_attribute(Hummingbird::Html::AttributeNames::Id); id && !id->empty()) {
        desc += "#" + *id;
    }
    if (const auto* cls = element->find_attribute(Hummingbird::Html::AttributeNames::Class); cls && !cls->empty()) {
        desc += "." + *cls;
    }
    if (const auto* type = element->find_attribute(Hummingbird::Html::AttributeNames::Type); type && !type->empty()) {
        desc += "[type=" + *type + "]";
    }
    return desc;
}

void set_input_value(DOM::Element& element, std::string_view value) {
    element.set_attribute(Hummingbird::Html::AttributeNames::Value, value);
}

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

std::optional<InputPaintData> build_input_paint_data(const DOM::Element& element, const Layout::RenderObject& node,
                                                     const Layout::Rect& absolute, IGraphicsContext& graphics) {
    const auto* style = node.get_computed_style();
    Layout::Metrics::Insets insets = Layout::Metrics::compute_insets(style);
    Layout::Rect content = {absolute.x + insets.left, absolute.y + insets.top,
                            absolute.width - insets.left - insets.right, absolute.height - insets.top - insets.bottom};
    if (content.width <= 0.0f || content.height <= 0.0f) {
        return std::nullopt;
    }

    TextStyle text_style = Layout::TextStyleUtils::build_text_style(style);
    if (style && style->background.has_value()) {
        const Color background = *style->background;
        if (text_style.color.a == 0 || low_contrast(text_style.color, background)) {
            text_style.color = high_contrast_text(background);
        }
    }
    std::string value = input_value(element);
    TextMetrics metrics = graphics.measure_text(value, text_style);
    TextMetrics caret_metrics =
        metrics.height > 0.0f ? metrics : graphics.measure_text(kCaretFallbackGlyph, text_style);
    float text_height = metrics.height > 0.0f ? metrics.height : caret_metrics.height;
    float text_x = content.x;
    float text_y = content.y + std::max(0.0f, (content.height - text_height) * 0.5f);
    if (!is_editable_input_element(&element)) {
        text_style.bold = true;
        text_x = content.x + std::max(0.0f, (content.width - metrics.width) * 0.5f);
    }

    return InputPaintData{
        absolute, content, std::move(text_style), std::move(value), text_x, text_y, text_height,
    };
}

void paint_input_value(const InputPaintData& data, IGraphicsContext& graphics) {
    if (!data.value.empty()) {
        graphics.draw_text(data.value, data.text_x, data.text_y, data.text_style);
    }
}

void paint_input_caret(const InputPaintData& data, IGraphicsContext& graphics, size_t caret, float scroll_y,
                       bool repaint_background) {
    caret = Core::Utils::TextEditBuffer::clamp_caret_for(data.value, caret);
    std::string prefix = data.value.substr(0, caret);
    float caret_offset = graphics.measure_text(prefix, data.text_style).width;
    float caret_x = data.text_x + caret_offset;
    float max_caret_x = data.content.x + std::max(0.0f, data.content.width - 1.0f);
    if (caret_x > max_caret_x) {
        caret_x = max_caret_x;
    }
    Layout::Rect caret_rect{caret_x, data.text_y, kCaretWidth, data.text_height};
    graphics.fill_rect(caret_rect, data.text_style.color);
    HB_LOG_DEBUG("[input] paint focused rect=" << data.absolute.x << "," << data.absolute.y << " "
                                               << data.absolute.width << "x" << data.absolute.height
                                               << " content=" << data.content.x << "," << data.content.y << " "
                                               << data.content.width << "x" << data.content.height
                                               << " scroll_y=" << scroll_y << " repaint_bg=" << repaint_background);
}

void paint_input_focus_ring(const Layout::Rect& absolute, IGraphicsContext& graphics) {
    constexpr float kStroke = 1.0f;
    graphics.fill_rect({absolute.x, absolute.y, absolute.width, kStroke}, kFocusRingColor);
    graphics.fill_rect({absolute.x, absolute.y + absolute.height - kStroke, absolute.width, kStroke}, kFocusRingColor);
    graphics.fill_rect({absolute.x, absolute.y, kStroke, absolute.height}, kFocusRingColor);
    graphics.fill_rect({absolute.x + absolute.width - kStroke, absolute.y, kStroke, absolute.height}, kFocusRingColor);
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

            if (repaint_background) {
                node.paint_self(graphics, local_offset);
            }

            auto paint_data = build_input_paint_data(*element, node, absolute, graphics);
            if (!paint_data.has_value()) {
                return Layout::Traversal::TraverseAction::Continue;
            }

            paint_input_value(*paint_data, graphics);

            if (element == focused_input_) {
                paint_input_focus_ring(absolute, graphics);
                HB_LOG_DEBUG("[input] draw focused value='"
                             << paint_data->value << "' text_pos=" << paint_data->text_x << "," << paint_data->text_y
                             << " text_h=" << paint_data->text_height << " color=("
                             << static_cast<int>(paint_data->text_style.color.r) << ","
                             << static_cast<int>(paint_data->text_style.color.g) << ","
                             << static_cast<int>(paint_data->text_style.color.b) << ","
                             << static_cast<int>(paint_data->text_style.color.a)
                             << ") font_size=" << paint_data->text_style.font_size
                             << " content=" << paint_data->content.x << "," << paint_data->content.y << " "
                             << paint_data->content.width << "x" << paint_data->content.height);
                paint_input_caret(*paint_data, graphics, caret_, scroll_y, repaint_background);
            }

            return Layout::Traversal::TraverseAction::Continue;
        });
}

}  // namespace Hummingbird::Engine
