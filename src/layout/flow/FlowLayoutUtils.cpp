#include "layout/flow/FlowLayoutUtils.h"

#include <algorithm>

#include "layout/geometry/PositioningUtils.h"

namespace Hummingbird::Layout::FlowLayout {

ChildMargins compute_child_margins(const Css::ComputedStyle* style, bool allow_auto) {
    ChildMargins margins;
    if (!style) {
        return margins;
    }
    margins.left = style->margin.left;
    margins.right = style->margin.right;
    margins.top = style->margin.top;
    margins.bottom = style->margin.bottom;
    if (allow_auto) {
        margins.left_auto = style->margin_left_auto;
        margins.right_auto = style->margin_right_auto;
    }
    return margins;
}

Css::ComputedStyle::Float resolve_float_type(RenderObject& child, bool ignore_absolute) {
    const auto* style = child.get_computed_style();
    if (ignore_absolute && Positioning::is_absolute(style)) {
        return Css::ComputedStyle::Float::None;
    }
    if (style && style->float_type != Css::ComputedStyle::Float::None) {
        return style->float_type;
    }
    if (!child.Inline()) {
        return Css::ComputedStyle::Float::None;
    }
    if (child.get_children().size() != 1) {
        return Css::ComputedStyle::Float::None;
    }
    const auto* grand_style = child.get_children()[0]->get_computed_style();
    if (grand_style && grand_style->float_type != Css::ComputedStyle::Float::None) {
        return grand_style->float_type;
    }
    return Css::ComputedStyle::Float::None;
}

void flush_line(LineCursor& cursor, float cursor_left) {
    cursor.y += cursor.line_height;
    cursor.x = cursor_left;
    cursor.line_height = 0.0f;
}

void layout_block_child(IGraphicsContext& context, RenderObject& child, const ChildMargins& margins, LineCursor& cursor,
                        float content_left, float content_width) {
    if (margins.top > 0.0f) {
        cursor.y += margins.top;
    }
    flush_line(cursor, content_left);
    float child_x = content_left + margins.left;
    float child_y = cursor.y;
    float available_width =
        content_width - (margins.left_auto ? 0.0f : margins.left) - (margins.right_auto ? 0.0f : margins.right);
    if (available_width < 0.0f) {
        available_width = 0.0f;
    }
    Rect child_bounds = {child_x, child_y, available_width, 0.0f};
    child.layout(context, child_bounds);
    if (margins.left_auto || margins.right_auto) {
        float remaining = content_width - child.get_rect().width - (margins.left_auto ? 0.0f : margins.left) -
                          (margins.right_auto ? 0.0f : margins.right);
        if (remaining < 0.0f) {
            remaining = 0.0f;
        }
        float left_margin = margins.left;
        float right_margin = margins.right;
        if (margins.left_auto && margins.right_auto) {
            left_margin = remaining * 0.5f;
            right_margin = remaining * 0.5f;
        } else if (margins.left_auto) {
            left_margin = remaining;
        } else if (margins.right_auto) {
            right_margin = remaining;
        }
        child_x = content_left + left_margin;
        child.set_rect({child_x, child.get_rect().y, child.get_rect().width, child.get_rect().height});
    }
    cursor.y = child_y + child.get_rect().height + margins.bottom;
}

void layout_float_child(IGraphicsContext& context, RenderObject& child, const ChildMargins& margins, LineCursor& cursor,
                        Css::ComputedStyle::Float float_type, std::vector<FloatLayout::FloatBox>& floats,
                        float& max_float_bottom, float content_left, float content_right) {
    flush_line(cursor, content_left);

    float available_width = content_right - content_left - margins.left - margins.right;
    if (available_width < 0.0f) {
        available_width = 0.0f;
    }
    Rect child_bounds = {content_left, cursor.y, available_width, 0.0f};
    child.layout(context, child_bounds);

    FloatLayout::FloatPlacement placement =
        FloatLayout::place_float(floats, float_type, cursor.y, child.get_rect().width, child.get_rect().height,
                                 margins.left, margins.right, margins.top, margins.bottom, content_left, content_right);
    child.set_rect(placement.rect);
    floats.push_back({placement.margin_rect, float_type});
    max_float_bottom = std::max(max_float_bottom, placement.margin_rect.y + placement.margin_rect.height);
}

InlineLayout::InlineLayoutResult layout_inline_group(IGraphicsContext& context,
                                                     std::vector<std::unique_ptr<RenderObject>>& children, size_t& i,
                                                     LineCursor& cursor, float base_x, float content_width,
                                                     Css::ComputedStyle::TextAlign text_align, float wrap_width,
                                                     bool capture_fragments) {
    InlineLayout::GroupLayoutContext layout_context;
    layout_context.start_x = cursor.x - base_x;
    layout_context.base_x = base_x;
    layout_context.base_y = cursor.y;
    layout_context.content_width = content_width;
    layout_context.align = text_align;
    layout_context.wrap_width = wrap_width;
    layout_context.capture_fragments = capture_fragments;
    InlineLayout::InlineLayoutResult result = InlineLayout::layout_inline_group(context, children, i, layout_context);

    InlineLayout::update_cursor_for_inline(cursor.x, cursor.y, cursor.line_height, layout_context.base_x,
                                           layout_context.base_y, result);
    return result;
}

}  // namespace Hummingbird::Layout::FlowLayout
