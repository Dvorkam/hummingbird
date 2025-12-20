#include "layout/RenderRule.h"
#include "core/IGraphicsContext.h"

namespace Hummingbird::Layout {

void RenderRule::layout(IGraphicsContext& /*context*/, const Rect& bounds) {
    const auto* style = get_computed_style();
    float h = style && style->height.has_value() ? *style->height : 2.0f;
    m_rect.x = bounds.x;
    m_rect.y = bounds.y;
    m_rect.width = bounds.width;
    m_rect.height = h;
}

void RenderRule::paint(IGraphicsContext& context, const Point& offset) {
    const auto* style = get_computed_style();
    Color c = style && style->background.has_value() ? *style->background : Color{50, 50, 50, 255};
    Rect rect{offset.x + m_rect.x, offset.y + m_rect.y, m_rect.width, m_rect.height};
    context.fill_rect(rect, c);
}

} // namespace Hummingbird::Layout
