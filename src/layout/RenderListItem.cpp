#include "layout/RenderListItem.h"

#include <algorithm>

#include "core/platform_api/IGraphicsContext.h"
#include "layout/LayoutMetricsUtils.h"
#include "layout/inline/InlineLayoutUtils.h"

namespace Hummingbird::Layout {

namespace {
struct LineCursor {
    float x;
    float y;
    float line_height;
};

struct ChildMargins {
    float left;
    float right;
    float top;
    float bottom;
};

ChildMargins compute_child_margins(const Css::ComputedStyle* style) {
    return {style ? style->margin.left : 0.0f, style ? style->margin.right : 0.0f, style ? style->margin.top : 0.0f,
            style ? style->margin.bottom : 0.0f};
}

void flush_line(LineCursor& cursor, float inset_left, float marker_offset) {
    cursor.y += cursor.line_height;
    cursor.x = inset_left + marker_offset;
    cursor.line_height = 0.0f;
}

void layout_block_child(IGraphicsContext& context, RenderObject& child, const ChildMargins& margins,
                        const Metrics::BoxMetrics& metrics, float marker_offset, LineCursor& cursor) {
    if (margins.top > 0.0f) {
        cursor.y += margins.top;
    }
    flush_line(cursor, metrics.insets.left, marker_offset);
    float child_x = metrics.insets.left + marker_offset + margins.left;
    float child_y = cursor.y;
    float available_width = metrics.content_width - margins.left - margins.right;
    Rect child_bounds = {child_x, child_y, available_width, 0.0f};
    child.layout(context, child_bounds);
    cursor.y = child_y + child.get_rect().height + margins.bottom;
}

InlineLayout::InlineLayoutResult layout_inline_group(IGraphicsContext& context,
                                                     std::vector<std::unique_ptr<RenderObject>>& children, size_t& i,
                                                     const Metrics::BoxMetrics& metrics, float marker_offset,
                                                     LineCursor& cursor, Css::ComputedStyle::TextAlign text_align,
                                                     float wrap_width) {
    InlineLayout::GroupLayoutContext layout_context;
    layout_context.start_x = cursor.x - (metrics.insets.left + marker_offset);
    layout_context.base_x = metrics.insets.left + marker_offset;
    layout_context.base_y = cursor.y;
    layout_context.content_width = metrics.content_width;
    layout_context.align = text_align;
    layout_context.wrap_width = wrap_width;
    layout_context.capture_fragments = true;
    InlineLayout::InlineLayoutResult result = InlineLayout::layout_inline_group(context, children, i, layout_context);

    InlineLayout::update_cursor_for_inline(cursor.x, cursor.y, cursor.line_height, layout_context.base_x,
                                           layout_context.base_y, result);
    return result;
}

void update_marker_for_block(bool& marker_y_set, float& marker_y, float inset_top) {
    if (marker_y_set) {
        return;
    }
    marker_y = inset_top;
    marker_y_set = true;
}

void update_marker_for_inline(const InlineLayout::InlineLayoutResult& inline_layout, bool& marker_y_set,
                              float& marker_y, float inset_top) {
    if (marker_y_set || inline_layout.heights.empty()) {
        return;
    }
    marker_y = inset_top + std::max(0.0f, (inline_layout.heights[0] - kListMarkerSizePx) * 0.5f);
    marker_y_set = true;
}
}  // namespace

RenderListItem::RenderListItem(const DOM::Node* dom_node) : BlockBox(dom_node) {
    m_marker = RenderMarker::create(dom_node);
}

const Rect& RenderListItem::marker_rect() const {
    return m_marker ? m_marker->get_rect() : m_rect;
}

void RenderListItem::layout(IGraphicsContext& context, const Rect& bounds) {
    const auto* style = get_computed_style();
    float marker_offset = kListMarkerSizePx + kListMarkerGapPx;
    Metrics::BoxMetrics metrics =
        Metrics::compute_box_metrics(style, bounds, m_rect, Metrics::BoxWidthPolicy::WidthOnly, marker_offset);
    LineCursor cursor{metrics.insets.left + marker_offset, metrics.insets.top, 0.0f};
    float marker_y = metrics.insets.top;
    bool marker_y_set = false;

    size_t i = 0;
    while (i < m_children.size()) {
        auto& child = m_children[i];
        const auto* child_style = child->get_computed_style();
        ChildMargins margins = compute_child_margins(child_style);

        if (!child->Inline()) {
            layout_block_child(context, *child, margins, metrics, marker_offset, cursor);
            update_marker_for_block(marker_y_set, marker_y, metrics.insets.top);
            ++i;
            continue;
        }

        auto align = style ? style->text_align : Css::ComputedStyle::TextAlign::Left;
        float wrap_width =
            (style && style->whitespace == Css::ComputedStyle::WhiteSpace::NoWrap) ? 0.0f : metrics.content_width;
        InlineLayout::InlineLayoutResult inline_layout =
            layout_inline_group(context, m_children, i, metrics, marker_offset, cursor, align, wrap_width);
        update_marker_for_inline(inline_layout, marker_y_set, marker_y, metrics.insets.top);
    }

    flush_line(cursor, metrics.insets.left, marker_offset);
    m_rect.height = cursor.y + metrics.insets.bottom;

    if (m_marker) {
        Rect marker_bounds{metrics.insets.left, marker_y, kListMarkerSizePx, kListMarkerSizePx};
        m_marker->layout(context, marker_bounds);
    }
}

void RenderListItem::paint_self(IGraphicsContext& context, const Point& offset) const {
    if (m_marker) {
        Point marker_offset{offset.x + m_rect.x, offset.y + m_rect.y};
        m_marker->paint(context, marker_offset);
    }
    RenderObject::paint_self(context, offset);
}

void RenderMarker::layout(IGraphicsContext& /*context*/, const Rect& bounds) {
    m_rect = bounds;
    m_rect.width = m_size;
    m_rect.height = m_size;
}

void RenderMarker::paint_self(IGraphicsContext& context, const Point& offset) const {
    const auto* style = get_computed_style();
    Color color = style ? style->color : Color{0, 0, 0, 255};
    Rect absolute{offset.x + m_rect.x, offset.y + m_rect.y, m_rect.width, m_rect.height};
    context.fill_rect(absolute, color);
}

}  // namespace Hummingbird::Layout
