#include "layout/RenderObject.h"

#include "core/IGraphicsContext.h"

namespace Hummingbird::Layout {

void RenderObject::layout(IGraphicsContext& context, const Rect& bounds) {
    m_rect = bounds;
}

void RenderObject::paint(IGraphicsContext& context, const Point& offset) {
    const auto* style = get_computed_style();
    if (style && style->border_style == Css::ComputedStyle::BorderStyle::Solid) {
        Layout::Rect absolute{offset.x + m_rect.x, offset.y + m_rect.y, m_rect.width, m_rect.height};
        const auto& bw = style->border_width;
        const auto& color = style->border_color;

        if (bw.top > 0.0f) {
            Layout::Rect top{absolute.x, absolute.y, absolute.width, bw.top};
            context.fill_rect(top, color);
        }
        if (bw.bottom > 0.0f) {
            Layout::Rect bottom{absolute.x, absolute.y + absolute.height - bw.bottom, absolute.width, bw.bottom};
            context.fill_rect(bottom, color);
        }
        if (bw.left > 0.0f) {
            Layout::Rect left{absolute.x, absolute.y, bw.left, absolute.height};
            context.fill_rect(left, color);
        }
        if (bw.right > 0.0f) {
            Layout::Rect right{absolute.x + absolute.width - bw.right, absolute.y, bw.right, absolute.height};
            context.fill_rect(right, color);
        }
    }

    for (auto& child : m_children) {
        Point child_offset = {offset.x + m_rect.x, offset.y + m_rect.y};
        child->paint(context, child_offset);
    }
}

}  // namespace Hummingbird::Layout
