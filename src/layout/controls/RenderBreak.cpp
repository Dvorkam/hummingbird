#include "layout/controls/RenderBreak.h"

#include "layout/Geometry.h"
#include "style/compute/ComputedStyle.h"

namespace Hummingbird::Layout {

namespace {
constexpr float kDefaultLineHeight = 16.0f;
}

void RenderBreak::layout(IGraphicsContext& /*context*/, const Rect& bounds) {
    const auto* style = get_computed_style();
    float line_height = style ? style->font_size : kDefaultLineHeight;
    if (style && style->line_height > 0.0f) {
        line_height = style->line_height;
    }
    m_rect.x = bounds.x;
    m_rect.y = bounds.y;
    m_rect.width = 0.0f;
    m_rect.height = line_height;
}

void RenderBreak::paint_self(IGraphicsContext& /*context*/, const Point& /*offset*/) const {
    // No-op; this is a control object for layout only.
}

}  // namespace Hummingbird::Layout
