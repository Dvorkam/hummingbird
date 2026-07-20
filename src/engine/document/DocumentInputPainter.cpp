#include "engine/document/DocumentInputPainter.h"

#include <algorithm>
#include <cmath>
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
#include "layout/paint/PaintUtils.h"
#include "style/types/ComputedStyle.h"

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

// Inputs with `background: none` (DDG) paint nothing themselves, so erasing a
// deleted character needs the backdrop actually behind the control: the
// nearest ancestor with an opaque background, white as the last resort.
// The fill must stay inside that ancestor's padding box: chromeless inputs may
// intentionally overflow their styled container (DDG's input pokes past the
// form's border box), and an unclipped fill wipes the container's border and
// shadow from the cached document underneath.
struct Backdrop {
    Color color{255, 255, 255, 255};
    std::optional<Layout::Rect> clip;
};

Backdrop resolve_backdrop(const Layout::RenderObject& node, const Layout::Rect& absolute) {
    Layout::Rect current_abs = absolute;
    for (const auto* current = &node; current; current = current->get_parent()) {
        const auto* style = current->get_computed_style();
        if (style && style->background.has_value() && style->background->a == 255) {
            Layout::Rect padding_box{current_abs.x + style->border_width.left, current_abs.y + style->border_width.top,
                                     current_abs.width - style->border_width.left - style->border_width.right,
                                     current_abs.height - style->border_width.top - style->border_width.bottom};
            return {*style->background, padding_box};
        }
        // Hoist this box's rect into the parent's coordinate space before
        // walking up (child rects are parent-relative).
        const Layout::Rect relative = current->get_rect();
        const Layout::RenderObject* parent = current->get_parent();
        if (!parent) break;
        const Layout::Rect parent_relative = parent->get_rect();
        current_abs = {current_abs.x - relative.x, current_abs.y - relative.y, parent_relative.width,
                       parent_relative.height};
    }
    return {};
}

Layout::Rect intersect_rects(const Layout::Rect& a, const Layout::Rect& b) {
    float left = std::max(a.x, b.x);
    float top = std::max(a.y, b.y);
    float right = std::min(a.x + a.width, b.x + b.width);
    float bottom = std::min(a.y + a.height, b.y + b.height);
    return {left, top, std::max(0.0f, right - left), std::max(0.0f, bottom - top)};
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

// Pages that strip the input's native border (border:none) are drawing their
// own control chrome; a synthetic focus ring on top of it fights their design.
bool wants_synthetic_focus_ring(const Css::ComputedStyle* style) {
    if (!style) return true;
    if (style->border_style == Css::ComputedStyle::BorderStyle::None) return false;
    const auto& width = style->border_width;
    return width.top > 0.0f || width.right > 0.0f || width.bottom > 0.0f || width.left > 0.0f;
}

void paint_input_focus_ring(const Layout::Rect& absolute, const Css::ComputedStyle* style, IGraphicsContext& graphics) {
    constexpr float kStroke = 1.0f;
    // Follow the control's corner radii so the ring hugs a rounded field instead
    // of showing sharp corners (reuses the rounded-stroke helper the CSS `outline`
    // painter uses; ResolvedCorners of all-zero degrade to a plain rectangle).
    const Layout::PaintUtils::ResolvedCorners corners =
        style ? Layout::PaintUtils::resolve_corners(style->border_radius, absolute.width, absolute.height)
              : Layout::PaintUtils::ResolvedCorners{};
    Layout::PaintUtils::draw_rounded_border_corners(graphics, absolute, corners, kStroke, kFocusRingColor);
}

// Draws a checkmark inside `box` as two stroked segments (axis-aligned fill_rect
// dabs stepped along each segment, since that is the only primitive available).
void draw_checkmark(const Layout::Rect& box, const Color& color, IGraphicsContext& graphics) {
    const float s = std::min(box.width, box.height);
    const float dab = std::max(1.5f, s * 0.13f);
    const auto pt = [&](float fx, float fy) { return Layout::Point{box.x + fx * s, box.y + fy * s}; };
    const auto stroke = [&](Layout::Point p0, Layout::Point p1) {
        const int steps = static_cast<int>(std::max(std::fabs(p1.x - p0.x), std::fabs(p1.y - p0.y))) + 1;
        for (int i = 0; i <= steps; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(steps);
            const float x = p0.x + (p1.x - p0.x) * t;
            const float y = p0.y + (p1.y - p0.y) * t;
            graphics.fill_rect({x - dab * 0.5f, y - dab * 0.5f, dab, dab}, color);
        }
    };
    stroke(pt(0.22f, 0.52f), pt(0.42f, 0.72f));  // short down-stroke
    stroke(pt(0.42f, 0.72f), pt(0.80f, 0.26f));  // long up-stroke
}

void paint_checkbox_control(const DOM::Element& element, const Layout::RenderObject& node, const Layout::Rect& absolute,
                            const Layout::Point& local_offset, IGraphicsContext& graphics, bool repaint_background) {
    if (repaint_background) {
        Backdrop backdrop = resolve_backdrop(node, absolute);
        Layout::Rect fill = backdrop.clip ? intersect_rects(absolute, *backdrop.clip) : absolute;
        if (fill.width > 0.0f && fill.height > 0.0f) {
            graphics.fill_rect(fill, backdrop.color);
        }
        node.paint_self(graphics, local_offset);
    }

    const bool checked = is_checkbox_checked(element);
    const Color white{255, 255, 255, 255};
    const Color accent{66, 133, 244, 255};
    const Color border{140, 149, 168, 255};  // unchecked border
    const Color inset_shadow{214, 219, 229, 255};

    // White box in both states; a blue border + blue checkmark indicate checked.
    graphics.fill_rect(absolute, checked ? accent : border);
    Layout::Rect inner{absolute.x + 1.0f, absolute.y + 1.0f, std::max(0.0f, absolute.width - 2.0f),
                       std::max(0.0f, absolute.height - 2.0f)};
    if (inner.width > 0.0f && inner.height > 0.0f) {
        graphics.fill_rect(inner, white);
        // A little depth: a subtle darker edge along the inside bottom.
        if (inner.height >= 3.0f) {
            graphics.fill_rect({inner.x, inner.y + inner.height - 1.0f, inner.width, 1.0f}, inset_shadow);
        }
    }
    if (checked) {
        draw_checkmark(absolute, accent, graphics);
    }
}

}  // namespace

void paint_input_control(const DOM::Element& element, const Layout::RenderObject& node, const Layout::Rect& absolute,
                         const Layout::Point& local_offset, IGraphicsContext& graphics, bool repaint_background,
                         bool focused, size_t caret, float scroll_y) {
    // A checkbox renders as a box + checkmark, not as a text field (7.2.6).
    if (is_checkbox_input_element(&element)) {
        paint_checkbox_control(element, node, absolute, local_offset, graphics, repaint_background);
        return;
    }
    if (repaint_background) {
        Backdrop backdrop = resolve_backdrop(node, absolute);
        Layout::Rect fill = backdrop.clip ? intersect_rects(absolute, *backdrop.clip) : absolute;
        if (fill.width > 0.0f && fill.height > 0.0f) {
            graphics.fill_rect(fill, backdrop.color);
        }
        node.paint_self(graphics, local_offset);
    }

    auto paint_data = build_input_paint_data(element, node, absolute, graphics);
    if (!paint_data.has_value()) {
        return;
    }

    paint_input_value(*paint_data, graphics);

    if (focused) {
        if (wants_synthetic_focus_ring(node.get_computed_style())) {
            paint_input_focus_ring(absolute, node.get_computed_style(), graphics);
        }
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
