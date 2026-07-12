#include "layout/controls/RenderBreak.h"

#include "layout/geometry/Geometry.h"
#include "layout/geometry/metrics/TextMetricsUtils.h"
#include "style/types/ComputedStyle.h"

namespace Hummingbird::Layout {

namespace {
constexpr float kDefaultLineHeight = 16.0f;
}

void RenderBreak::layout(IGraphicsContext& /*context*/, const Rect& bounds) {
    const auto* style = get_computed_style();
    float fallback = style ? style->font_size : kDefaultLineHeight;
    float line_height = TextMetricsUtils::resolve_line_height(style, fallback);
    m_rect.x = bounds.x;
    m_rect.y = bounds.y;
    m_rect.width = 0.0f;
    m_rect.height = line_height;
}

void RenderBreak::paint_self(IGraphicsContext& /*context*/, const Point& /*offset*/) const {
    // No-op; this is a control object for layout only.
}

}  // namespace Hummingbird::Layout
