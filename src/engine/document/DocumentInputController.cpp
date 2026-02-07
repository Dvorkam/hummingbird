#include "engine/document/DocumentInputController.h"

#include <algorithm>

#include "core/dom/Element.h"
#include "core/utils/TextEditBuffer.h"
#include "engine/document/HitTestUtils.h"
#include "html/HtmlAttributeNames.h"
#include "html/HtmlTagNames.h"
#include "layout/RenderObject.h"
#include "layout/flow/TextStyleUtils.h"
#include "layout/geometry/GeometryUtils.h"
#include "layout/geometry/metrics/LayoutMetricsUtils.h"

namespace Hummingbird::Engine {

namespace {
constexpr float kCaretWidth = 1.0f;
constexpr const char* kCaretFallbackGlyph = "A";

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

std::string input_value(const DOM::Element& element) {
    if (const auto* value = element.find_attribute(Hummingbird::Html::AttributeNames::Value)) {
        return *value;
    }
    return {};
}

void set_input_value(DOM::Element& element, std::string_view value) {
    element.set_attribute(Hummingbird::Html::AttributeNames::Value, value);
}

DOM::Element* hit_test_input(const Layout::RenderObject* render_tree, const Layout::Point& point,
                             const Layout::Rect& viewport, float scroll_y) {
    auto hit = HitTest::hit_test_z_order<DOM::Element*>(
        render_tree, point, viewport, scroll_y, [&](const Layout::RenderObject& node) -> std::optional<DOM::Element*> {
            auto* element = dynamic_cast<const DOM::Element*>(node.get_dom_node());
            if (!is_input_element(element)) {
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
    std::string value = input_value(element);
    TextMetrics metrics = graphics.measure_text(value, text_style);
    TextMetrics caret_metrics =
        metrics.height > 0.0f ? metrics : graphics.measure_text(kCaretFallbackGlyph, text_style);
    float text_height = metrics.height > 0.0f ? metrics.height : caret_metrics.height;
    float text_x = content.x;
    float text_y = content.y + std::max(0.0f, (content.height - text_height) * 0.5f);

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
}  // namespace

void DocumentInputController::reset() {
    focused_input_ = nullptr;
    caret_ = 0;
}

bool DocumentInputController::focus_input_at(const Layout::RenderObject* render_tree, const Layout::Point& point,
                                             const Layout::Rect& viewport, float scroll_y) {
    DOM::Element* hit = hit_test_input(render_tree, point, viewport, scroll_y);
    if (hit == focused_input_) {
        caret_ = focused_input_ ? input_value(*focused_input_).size() : 0;
        return focused_input_ != nullptr;
    }
    focused_input_ = hit;
    caret_ = focused_input_ ? input_value(*focused_input_).size() : 0;
    return focused_input_ != nullptr;
}

bool DocumentInputController::clear_focus() {
    if (!focused_input_) {
        return false;
    }
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
                paint_input_caret(*paint_data, graphics, caret_, scroll_y, repaint_background);
            }

            return Layout::Traversal::TraverseAction::Continue;
        });
}

}  // namespace Hummingbird::Engine
