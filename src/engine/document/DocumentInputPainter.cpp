#include "engine/document/DocumentInputPainter.h"

#include <algorithm>
#include <cstdlib>
#include <optional>
#include <string>

#include "core/dom/Element.h"
#include "core/utils/Log.h"
#include "core/utils/TextEditBuffer.h"
#include "engine/document/DocumentInputUtils.h"
#include "layout/RenderObject.h"
#include "layout/flow/TextStyleUtils.h"
#include "layout/geometry/metrics/LayoutMetricsUtils.h"

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

void paint_input_control(const DOM::Element& element, const Layout::RenderObject& node, const Layout::Rect& absolute,
                         const Layout::Point& local_offset, IGraphicsContext& graphics, bool repaint_background,
                         bool focused, size_t caret, float scroll_y) {
    if (repaint_background) {
        node.paint_self(graphics, local_offset);
    }

    auto paint_data = build_input_paint_data(element, node, absolute, graphics);
    if (!paint_data.has_value()) {
        return;
    }

    paint_input_value(*paint_data, graphics);

    if (focused) {
        paint_input_focus_ring(absolute, graphics);
        HB_LOG_DEBUG("[input] draw focused value='" << paint_data->value << "' text_pos=" << paint_data->text_x << ","
                                                    << paint_data->text_y << " text_h=" << paint_data->text_height
                                                    << " color=(" << static_cast<int>(paint_data->text_style.color.r)
                                                    << "," << static_cast<int>(paint_data->text_style.color.g) << ","
                                                    << static_cast<int>(paint_data->text_style.color.b) << ","
                                                    << static_cast<int>(paint_data->text_style.color.a)
                                                    << ") font_size=" << paint_data->text_style.font_size << " content="
                                                    << paint_data->content.x << "," << paint_data->content.y << " "
                                                    << paint_data->content.width << "x" << paint_data->content.height);
        paint_input_caret(*paint_data, graphics, caret, scroll_y, repaint_background);
    }
}

}  // namespace Hummingbird::Engine
