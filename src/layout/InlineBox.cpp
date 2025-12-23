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
    float border_left = style ? style->border_width.left : 0.0f;
    float border_right = style ? style->border_width.right : 0.0f;
    float border_top = style ? style->border_width.top : 0.0f;
    float border_bottom = style ? style->border_width.bottom : 0.0f;

    float inset_left = padding_left + border_left;
    float inset_right = padding_right + border_right;
    float inset_top = padding_top + border_top;
    float inset_bottom = padding_bottom + border_bottom;

    float content_width = bounds.width - inset_left - inset_right;
    if (content_width < 0.0f) content_width = 0.0f;

    float cursor_x = inset_left;
    float cursor_y = inset_top;
    float line_height = 0.0f;

    for (auto& child : m_children) {
        const auto* child_style = child->get_computed_style();
        float margin_left = child_style ? child_style->margin.left : 0.0f;
        float margin_right = child_style ? child_style->margin.right : 0.0f;
        float margin_top = child_style ? child_style->margin.top : 0.0f;
        float margin_bottom = child_style ? child_style->margin.bottom : 0.0f;

        float child_x = cursor_x + margin_left;
        float child_y = cursor_y + margin_top;
        float available_width = content_width - (child_x - inset_left);
        Rect child_bounds{child_x, child_y, available_width, 0.0f};
        child->layout(context, child_bounds);

        float child_height = child->get_rect().height + margin_top + margin_bottom;
        line_height = std::max(line_height, child_height);
        cursor_x = child_x + child->get_rect().width + margin_right;
    }

    m_rect.width = cursor_x + inset_right;
    m_rect.height = cursor_y + line_height + inset_bottom;
}

void InlineBox::paint(IGraphicsContext& context, const Point& offset) {
    RenderObject::paint(context, offset);
}

}  // namespace Hummingbird::Layout
