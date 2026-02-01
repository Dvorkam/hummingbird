#include "layout/RenderRule.h"

#include <algorithm>
#include <optional>

#include "core/platform_api/IGraphicsContext.h"
#include "layout/Geometry.h"
#include "style/ComputedStyle.h"

namespace Hummingbird::Layout {

namespace {
constexpr float kDefaultRuleHeight = 2.0f;
}  // namespace

void RenderRule::layout(IGraphicsContext& /*context*/, const Rect& bounds) {
    const auto* style = get_computed_style();
    float width = bounds.width;
    if (style && style->width.has_value()) {
        width = *style->width;
    }
    if (style && style->min_width.has_value()) {
        width = std::max(width, *style->min_width);
    }
    if (style && style->max_width.has_value()) {
        width = std::min(width, *style->max_width);
    }
    if (width < 0.0f) {
        width = 0.0f;
    }

    float h = kDefaultRuleHeight;
    if (style && style->height.has_value()) {
        h = *style->height;
    } else if (style && style->border_style == Css::ComputedStyle::BorderStyle::Solid) {
        const auto& bw = style->border_width;
        float border_height = bw.top + bw.bottom;
        if (border_height > 0.0f) {
            h = border_height;
        }
    }
    if (style && style->min_height.has_value()) {
        h = std::max(h, *style->min_height);
    }
    if (style && style->max_height.has_value()) {
        h = std::min(h, *style->max_height);
    }
    if (h < 0.0f) {
        h = 0.0f;
    }
    m_rect.x = bounds.x;
    m_rect.y = bounds.y;
    m_rect.width = width;
    m_rect.height = h;
}

void RenderRule::paint_self(IGraphicsContext& context, const Point& offset) const {
    const auto* style = get_computed_style();
    bool has_background = style && style->background.has_value();
    bool has_border = false;
    if (style && style->border_style == Css::ComputedStyle::BorderStyle::Solid) {
        const auto& bw = style->border_width;
        has_border = bw.top > 0.0f || bw.right > 0.0f || bw.bottom > 0.0f || bw.left > 0.0f;
    }

    if (has_background || has_border) {
        RenderObject::paint_self(context, offset);
        return;
    }

    Rect rect{offset.x + m_rect.x, offset.y + m_rect.y, m_rect.width, m_rect.height};
    context.fill_rect(rect, Color{50, 50, 50, 255});
}

}  // namespace Hummingbird::Layout
