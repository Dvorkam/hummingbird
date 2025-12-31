#include "layout/RenderRule.h"

#include "core/platform_api/IGraphicsContext.h"

namespace Hummingbird::Layout {

namespace {
constexpr float kDefaultRuleHeight = 2.0f;
constexpr Color kDefaultRuleColor{50, 50, 50, 255};
}  // namespace

void RenderRule::layout(IGraphicsContext& /*context*/, const Rect& bounds) {
    const auto* style = get_computed_style();
    float h = style && style->height.has_value() ? *style->height : kDefaultRuleHeight;
    m_rect.x = bounds.x;
    m_rect.y = bounds.y;
    m_rect.width = bounds.width;
    m_rect.height = h;
}

void RenderRule::paint_self(IGraphicsContext& context, const Point& offset) {
    const auto* style = get_computed_style();
    Color c = style && style->background.has_value() ? *style->background : kDefaultRuleColor;
    Rect rect{offset.x + m_rect.x, offset.y + m_rect.y, m_rect.width, m_rect.height};
    context.fill_rect(rect, c);
}

}  // namespace Hummingbird::Layout
