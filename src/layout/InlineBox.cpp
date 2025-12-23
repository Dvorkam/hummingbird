#include "layout/InlineBox.h"

#include <algorithm>

#include "core/IGraphicsContext.h"

namespace Hummingbird::Layout {

void InlineBox::layout(IGraphicsContext& context, const Rect& bounds) {
    m_rect.x = bounds.x;
    m_rect.y = bounds.y;

    const auto* style = get_computed_style();
    float padding_left = style ? style->padding.left : 0.0f;
    float padding_right = style ? style->padding.right : 0.0f;
    float padding_top = style ? style->padding.top : 0.0f;
    float padding_bottom = style ? style->padding.bottom : 0.0f;

    float cursor_x = padding_left;
    float cursor_y = padding_top;
    float line_height = 0.0f;

    for (auto& child : m_children) {
        const auto* child_style = child->get_computed_style();
        float margin_left = child_style ? child_style->margin.left : 0.0f;
        float margin_right = child_style ? child_style->margin.right : 0.0f;
        float margin_top = child_style ? child_style->margin.top : 0.0f;
        float margin_bottom = child_style ? child_style->margin.bottom : 0.0f;

        float child_x = cursor_x + margin_left;
        float child_y = cursor_y + margin_top;
        Rect child_bounds{child_x, child_y, bounds.width - (child_x - bounds.x), 0.0f};
        child->layout(context, child_bounds);

        float child_height = child->get_rect().height + margin_top + margin_bottom;
        line_height = std::max(line_height, child_height);
        cursor_x = child_x + child->get_rect().width + margin_right;
    }

    m_rect.width = cursor_x + padding_right;
    m_rect.height = cursor_y + line_height + padding_bottom;
}

void InlineBox::paint(IGraphicsContext& context, const Point& offset) {
    RenderObject::paint(context, offset);
}

}  // namespace Hummingbird::Layout
