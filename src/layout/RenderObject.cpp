#include "layout/RenderObject.h"

#include <optional>

#include "core/platform_api/IGraphicsContext.h"
#include "core/platform_api/IImageDecoder.h"
#include "layout/paint/PaintUtils.h"
#include "style/compute/ComputedStyle.h"

namespace Hummingbird::Layout {

void RenderObject::layout(IGraphicsContext& context, const Rect& bounds) {
    m_rect = bounds;
}

void RenderObject::paint(IGraphicsContext& context, const Point& offset) const {
    const auto* style = get_computed_style();
    Point paint_offset = offset;
    if (style && style->transform_has_translate) {
        paint_offset.x += style->transform_translate_x;
        paint_offset.y += style->transform_translate_y;
    }
    paint_self(context, paint_offset);
    Point child_offset = {paint_offset.x + m_rect.x, paint_offset.y + m_rect.y};
    for (auto& child : m_children) {
        child->paint(context, child_offset);
    }
}

void RenderObject::paint_self(IGraphicsContext& context, const Point& offset) const {
    const auto* style = get_computed_style();
    Layout::Rect rect{offset.x + m_rect.x, offset.y + m_rect.y, m_rect.width, m_rect.height};
    PaintUtils::draw_box_decoration(context, rect, style, m_background_image);
}

}  // namespace Hummingbird::Layout
