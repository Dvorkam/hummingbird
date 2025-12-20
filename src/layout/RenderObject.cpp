#include "layout/RenderObject.h"
#include "core/IGraphicsContext.h"

namespace Hummingbird::Layout {

void RenderObject::layout(IGraphicsContext& context, const Rect& bounds) {
    m_rect = bounds;
}

void RenderObject::paint(IGraphicsContext& context, const Point& offset) {
    for (auto& child : m_children) {
        Point child_offset = { offset.x + m_rect.x, offset.y + m_rect.y };
        child->paint(context, child_offset);
    }
}

} // namespace Hummingbird::Layout
