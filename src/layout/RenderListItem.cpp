#include "layout/RenderListItem.h"

#include <algorithm>

#include "core/platform_api/IGraphicsContext.h"
#include "layout/InlineLineBuilder.h"

namespace Hummingbird::Layout {

namespace {
struct LayoutMetrics {
    float inset_left;
    float inset_right;
    float inset_top;
    float inset_bottom;
    float content_width;
    float marker_offset;
};

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

LayoutMetrics compute_metrics(const Css::ComputedStyle* style, const Rect& bounds, Rect& rect) {
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

    rect.x = bounds.x;
    rect.y = bounds.y;
    if (style && style->width.has_value()) {
        rect.width = target_width + inset_left + inset_right;
    } else {
        rect.width = bounds.width;
    }

    float marker_offset = kListMarkerSizePx + kListMarkerGapPx;
    float content_width = rect.width - inset_left - inset_right - marker_offset;
    if (content_width < 0.0f) content_width = 0.0f;

    return {inset_left, inset_right, inset_top, inset_bottom, content_width, marker_offset};
}

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
                        const LayoutMetrics& metrics, LineCursor& cursor) {
    if (margins.top > 0.0f) {
        cursor.y += margins.top;
    }
    flush_line(cursor, metrics.inset_left, metrics.marker_offset);
    float child_x = metrics.inset_left + metrics.marker_offset + margins.left;
    float child_y = cursor.y;
    float available_width = metrics.content_width - margins.left - margins.right;
    Rect child_bounds = {child_x, child_y, available_width, 0.0f};
    child.layout(context, child_bounds);
    cursor.y = child_y + child.get_rect().height + margins.bottom;
}

struct InlineLayoutResult {
    std::vector<InlineFragment> fragments;
    std::vector<float> heights;
    float last_line_width = 0.0f;
};

void measure_inline_participants(IGraphicsContext& context, std::vector<std::unique_ptr<RenderObject>>& children,
                                 size_t& i) {
    while (i < children.size()) {
        auto p = children[i]->Inline();
        if (!p) break;

        p.get().reset_inline_layout();
        p.get().measure_inline(context);
        ++i;
    }
}

void collect_inline_runs(IGraphicsContext& context, std::vector<std::unique_ptr<RenderObject>>& children, size_t& i,
                         std::vector<InlineRun>& runs) {
    while (i < children.size()) {
        auto p = children[i]->Inline();
        if (!p) break;

        p.get().collect_inline_runs(context, runs);
        ++i;
    }
}

void align_inline_lines(std::vector<InlineLine>& lines, float available_width, Css::ComputedStyle::TextAlign align) {
    if (align == Css::ComputedStyle::TextAlign::Left || available_width <= 0.0f) {
        return;
    }

    for (auto& line : lines) {
        if (line.fragments.empty()) {
            continue;
        }
        float min_x = line.fragments.front().rect.x;
        float max_x = line.fragments.front().rect.x + line.fragments.front().rect.width;
        for (const auto& fragment : line.fragments) {
            min_x = std::min(min_x, fragment.rect.x);
            max_x = std::max(max_x, fragment.rect.x + fragment.rect.width);
        }
        float line_width = max_x - min_x;
        if (line_width <= 0.0f || line_width >= available_width) {
            continue;
        }

        float desired_start = 0.0f;
        if (align == Css::ComputedStyle::TextAlign::Center) {
            desired_start = (available_width - line_width) * 0.5f;
        } else if (align == Css::ComputedStyle::TextAlign::Right) {
            desired_start = available_width - line_width;
        }
        float shift = desired_start - min_x;
        if (shift == 0.0f) {
            continue;
        }
        for (auto& fragment : line.fragments) {
            fragment.rect.x += shift;
        }
    }
}

InlineLayoutResult apply_inline_fragments(const std::vector<InlineLine>& lines, const std::vector<InlineRun>& runs,
                                          float base_x, float base_y) {
    InlineLayoutResult result;
    if (lines.empty()) {
        return result;
    }

    result.fragments.reserve(runs.size());
    result.heights.reserve(lines.size());
    size_t last_line = lines.size() - 1;

    for (const auto& line : lines) {
        result.heights.push_back(line.height);
        for (const auto& fragment : line.fragments) {
            InlineFragment resolved = fragment;
            resolved.rect.x += base_x;
            resolved.rect.y += base_y;
            result.fragments.push_back(resolved);
            auto& run = runs[resolved.run_index];
            if (run.owner) {
                if (auto p = run.owner->Inline()) {
                    p.get().apply_inline_fragment(run.local_index, resolved, run);
                }
            }
            if (resolved.line_index == last_line) {
                float extent = resolved.rect.x + resolved.rect.width - base_x;
                result.last_line_width = std::max(result.last_line_width, extent);
            }
        }
    }

    return result;
}

void update_cursor_for_inline(LineCursor& cursor, const LayoutMetrics& metrics, float base_y,
                              const InlineLayoutResult& result) {
    if (result.heights.empty()) {
        return;
    }

    float total_height = 0.0f;
    for (float h : result.heights) {
        total_height += h;
    }
    float last_height = result.heights.back();
    cursor.y = base_y + (total_height - last_height);
    cursor.x = metrics.inset_left + metrics.marker_offset + result.last_line_width;
    cursor.line_height = std::max(cursor.line_height, last_height);
}

InlineLayoutResult layout_inline_group(IGraphicsContext& context, std::vector<std::unique_ptr<RenderObject>>& children,
                                       size_t& i, const LayoutMetrics& metrics, LineCursor& cursor,
                                       Css::ComputedStyle::TextAlign text_align, float wrap_width) {
    InlineLayoutResult result;
    InlineLineBuilder builder;
    builder.reset();
    std::vector<InlineRun> runs;
    size_t group_start = i;
    size_t group_end = i;

    measure_inline_participants(context, children, group_end);
    i = group_start;
    collect_inline_runs(context, children, i, runs);

    if (runs.empty()) {
        return result;
    }

    for (const auto& run : runs) {
        builder.add_run(run);
    }

    float start_x = cursor.x - (metrics.inset_left + metrics.marker_offset);
    auto lines = builder.layout(wrap_width, start_x);
    if (lines.empty()) {
        return result;
    }
    align_inline_lines(lines, metrics.content_width, text_align);

    float base_x = metrics.inset_left + metrics.marker_offset;
    float base_y = cursor.y;
    result = apply_inline_fragments(lines, runs, base_x, base_y);

    for (size_t j = group_start; j < group_end; ++j) {
        if (auto p = children[j]->Inline()) {
            p.get().finalize_inline_layout();
        }
    }

    update_cursor_for_inline(cursor, metrics, base_y, result);
    return result;
}

void update_marker_for_block(bool& marker_y_set, float& marker_y, float inset_top) {
    if (marker_y_set) {
        return;
    }
    marker_y = inset_top;
    marker_y_set = true;
}

void update_marker_for_inline(const InlineLayoutResult& inline_layout, bool& marker_y_set, float& marker_y,
                              float inset_top) {
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
    LayoutMetrics metrics = compute_metrics(style, bounds, m_rect);
    LineCursor cursor{metrics.inset_left + metrics.marker_offset, metrics.inset_top, 0.0f};
    float marker_y = metrics.inset_top;
    bool marker_y_set = false;

    size_t i = 0;
    while (i < m_children.size()) {
        auto& child = m_children[i];
        const auto* child_style = child->get_computed_style();
        ChildMargins margins = compute_child_margins(child_style);

        if (!child->Inline()) {
            layout_block_child(context, *child, margins, metrics, cursor);
            update_marker_for_block(marker_y_set, marker_y, metrics.inset_top);
            ++i;
            continue;
        }

        auto align = style ? style->text_align : Css::ComputedStyle::TextAlign::Left;
        float wrap_width =
            (style && style->whitespace == Css::ComputedStyle::WhiteSpace::NoWrap) ? 0.0f : metrics.content_width;
        InlineLayoutResult inline_layout =
            layout_inline_group(context, m_children, i, metrics, cursor, align, wrap_width);
        update_marker_for_inline(inline_layout, marker_y_set, marker_y, metrics.inset_top);
    }

    flush_line(cursor, metrics.inset_left, metrics.marker_offset);
    m_rect.height = cursor.y + metrics.inset_bottom;

    if (m_marker) {
        Rect marker_bounds{metrics.inset_left, marker_y, kListMarkerSizePx, kListMarkerSizePx};
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
