#include "engine/document/DocumentInputController.h"

#include <algorithm>

#include "core/dom/Element.h"
#include "core/utils/Utf8Utils.h"
#include "html/HtmlAttributeNames.h"
#include "html/HtmlTagNames.h"
#include "layout/GeometryUtils.h"
#include "layout/metrics/LayoutMetricsUtils.h"
#include "layout/PositioningUtils.h"
#include "layout/RenderObject.h"
#include "layout/RenderTreeTraversal.h"
#include "layout/TextStyleUtils.h"

namespace Hummingbird::Engine {

namespace {
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
    if (!render_tree) {
        return nullptr;
    }
    if (!Layout::rect_contains_point(viewport, point)) {
        return nullptr;
    }

    Layout::Point offset{0.0f, -scroll_y};
    DOM::Element* result = nullptr;

    Layout::Positioning::traverse_render_tree_z_order(
        *render_tree, offset,
        [&](const Layout::RenderObject& node, const Layout::Rect& absolute, const Layout::Point& /*local_offset*/) {
            if (!Layout::rect_intersects(absolute, viewport) || !Layout::rect_contains_point(absolute, point)) {
                if (node.has_absolute_descendant()) {
                    return Layout::Traversal::TraverseAction::Continue;
                }
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
    caret_ = Core::Utils::clamp_caret(caret_, value);
    value.insert(caret_, text);
    caret_ += text.size();
    set_input_value(*focused_input_, value);

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
        if (!value.empty()) {
            caret_ = Core::Utils::clamp_caret(caret_, value);
            if (caret_ > 0) {
                auto start = Core::Utils::prev_codepoint(value, caret_);
                value.erase(start, caret_ - start);
                caret_ = start;
                set_input_value(*focused_input_, value);
            }
        }
        result.needs_repaint = true;
        return result;
    }

    if (event.key.key == Key::Delete) {
        result.handled = true;
        if (!value.empty()) {
            caret_ = Core::Utils::clamp_caret(caret_, value);
            if (caret_ < value.size()) {
                auto end = Core::Utils::next_codepoint(value, caret_);
                value.erase(caret_, end - caret_);
                set_input_value(*focused_input_, value);
            }
        }
        result.needs_repaint = true;
        return result;
    }

    if (event.key.key == Key::Left) {
        caret_ = Core::Utils::prev_codepoint(value, caret_);
        result.handled = true;
        result.needs_repaint = true;
        return result;
    }

    if (event.key.key == Key::Right) {
        caret_ = Core::Utils::next_codepoint(value, caret_);
        result.handled = true;
        result.needs_repaint = true;
        return result;
    }

    if (event.key.key == Key::Home) {
        caret_ = 0;
        result.handled = true;
        result.needs_repaint = true;
        return result;
    }

    if (event.key.key == Key::End) {
        caret_ = value.size();
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
                auto caret = Core::Utils::clamp_caret(caret_, value);
                std::string prefix = value.substr(0, caret);
                float caret_offset = graphics.measure_text(prefix, text_style).width;
                float caret_x = text_x + caret_offset;
                float max_caret_x = content.x + std::max(0.0f, content.width - 1.0f);
                if (caret_x > max_caret_x) {
                    caret_x = max_caret_x;
                }
                Layout::Rect caret_rect{caret_x, text_y, 1.0f, text_height};
                graphics.fill_rect(caret_rect, text_style.color);
                HB_LOG_DEBUG("[input] paint focused rect="
                             << absolute.x << "," << absolute.y << " " << absolute.width << "x" << absolute.height
                             << " content=" << content.x << "," << content.y << " " << content.width << "x"
                             << content.height << " scroll_y=" << scroll_y << " repaint_bg=" << repaint_background);
            }

            return Layout::Traversal::TraverseAction::Continue;
        });
}

}  // namespace Hummingbird::Engine
