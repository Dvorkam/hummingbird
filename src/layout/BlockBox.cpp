#include "layout/BlockBox.h"

#include <algorithm>

namespace Hummingbird::Layout {

void BlockBox::layout(IGraphicsContext& context, const Rect& bounds) {
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

    float target_width = bounds.width;
    if (style && style->width.has_value()) {
        target_width = std::min(bounds.width, *style->width);
    }

    m_rect.x = bounds.x;
    m_rect.y = bounds.y;
    if (style && style->width.has_value()) {
        m_rect.width = target_width + inset_left + inset_right;
    } else {
        m_rect.width = bounds.width;
    }

    float content_width = m_rect.width - inset_left - inset_right;
    float cursor_x = inset_left;
    float cursor_y = inset_top;
    float line_height = 0.0f;

    auto flush_line = [&]() {
        cursor_y += line_height;
        cursor_x = inset_left;
        line_height = 0.0f;
    };

    for (auto& child : m_children) {
        const auto* child_style = child->get_computed_style();
        float margin_left = child_style ? child_style->margin.left : 0.0f;
        float margin_right = child_style ? child_style->margin.right : 0.0f;
        float margin_top = child_style ? child_style->margin.top : 0.0f;
        float margin_bottom = child_style ? child_style->margin.bottom : 0.0f;

        bool is_inline = child->is_inline();

        if (!is_inline) {
            // Control objects like <br> need to break the line before stacking blocks.
            if (child_style && margin_top > 0.0f) {
                cursor_y += margin_top;
            }
            flush_line();
            float child_x = inset_left + margin_left;
            float child_y = cursor_y;
            float available_width = content_width - margin_left - margin_right;
            Rect child_bounds = {child_x, child_y, available_width, 0.0f};  // Height determined by child
            child->layout(context, child_bounds);
            cursor_y = child_y + child->get_rect().height + margin_bottom;
            continue;
        }

        float child_x = cursor_x + margin_left;
        float child_y = cursor_y + margin_top;
        float available_width = content_width - (child_x - inset_left) - margin_right;
        Rect child_bounds = {child_x, child_y, available_width, 0.0f};
        child->layout(context, child_bounds);

        // Greedy wrap: if this inline item overflows the available width, move to the next line and re-layout.
        float projected_right = child_x + child->get_rect().width + margin_right;
        float line_right = inset_left + content_width;
        if (projected_right > line_right && content_width > 0.0f) {
            flush_line();
            child_x = cursor_x + margin_left;
            child_y = cursor_y + margin_top;
            available_width = content_width - (child_x - inset_left) - margin_right;
            child_bounds = {child_x, child_y, available_width, 0.0f};
            child->layout(context, child_bounds);
            projected_right = child_x + child->get_rect().width + margin_right;
        }

        float child_height = child->get_rect().height + margin_top + margin_bottom;
        line_height = std::max(line_height, child_height);
        cursor_x = child_x + child->get_rect().width + margin_right;
    }

    flush_line();
    m_rect.height = cursor_y + inset_bottom;
}

void BlockBox::paint(IGraphicsContext& context, const Point& offset) {
    // A basic block box doesn't draw anything itself, it just contains other boxes.
    // The default implementation correctly paints the children.
    RenderObject::paint(context, offset);
}

void InlineBlockBox::layout(IGraphicsContext& context, const Rect& bounds) {
    BlockBox::layout(context, bounds);

    const auto* style = get_computed_style();
    if (style && style->width.has_value()) {
        return;
    }

    float padding_left = style ? style->padding.left : 0.0f;
    float padding_right = style ? style->padding.right : 0.0f;
    float border_left = style ? style->border_width.left : 0.0f;
    float border_right = style ? style->border_width.right : 0.0f;
    float inset_left = padding_left + border_left;
    float inset_right = padding_right + border_right;

    float content_right = inset_left;
    for (const auto& child : m_children) {
        const auto* child_style = child->get_computed_style();
        float margin_right = child_style ? child_style->margin.right : 0.0f;
        float right = child->get_rect().x + child->get_rect().width + margin_right;
        content_right = std::max(content_right, right);
    }

    float required_width = content_right + inset_right;
    if (required_width < inset_left + inset_right) {
        required_width = inset_left + inset_right;
    }

    m_rect.width = std::min(m_rect.width, required_width);
}

}  // namespace Hummingbird::Layout
