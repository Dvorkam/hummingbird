#include "layout/BlockBox.h"

namespace Hummingbird::Layout {

void BlockBox::layout(IGraphicsContext& context, const Rect& bounds) {
    const auto* style = get_computed_style();
    float padding_left = style ? style->padding.left : 0.0f;
    float padding_right = style ? style->padding.right : 0.0f;
    float padding_top = style ? style->padding.top : 0.0f;
    float padding_bottom = style ? style->padding.bottom : 0.0f;

    float target_width = bounds.width;
    if (style && style->width.has_value()) {
        target_width = std::min(bounds.width, *style->width);
    }

    m_rect.x = bounds.x;
    m_rect.y = bounds.y;
    m_rect.width = target_width;

    float current_y = padding_top;
    for (auto& child : m_children) {
        const auto* child_style = child->get_computed_style();
        float margin_left = child_style ? child_style->margin.left : 0.0f;
        float margin_right = child_style ? child_style->margin.right : 0.0f;
        float margin_top = child_style ? child_style->margin.top : 0.0f;
        float margin_bottom = child_style ? child_style->margin.bottom : 0.0f;

        float child_x = padding_left + margin_left;
        float child_y = current_y + margin_top;
        float available_width = m_rect.width - padding_left - padding_right - margin_left - margin_right;
        Rect child_bounds = {child_x, child_y, available_width, 0.0f}; // Height is determined by child
        child->layout(context, child_bounds);

        current_y = child_y + child->get_rect().height + margin_bottom;
    }

    m_rect.height = current_y + padding_bottom;
}

void BlockBox::paint(IGraphicsContext& context, const Point& offset) {
    // A basic block box doesn't draw anything itself, it just contains other boxes.
    // The default implementation correctly paints the children.
    RenderObject::paint(context, offset);
}

}
