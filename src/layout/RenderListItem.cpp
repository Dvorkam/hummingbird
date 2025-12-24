#include "layout/RenderListItem.h"

#include <algorithm>

#include "core/IGraphicsContext.h"
#include "layout/InlineLineBuilder.h"

namespace Hummingbird::Layout {

namespace {
constexpr float kMarkerSize = 6.0f;
constexpr float kMarkerGap = 6.0f;
}  // namespace

RenderListItem::RenderListItem(const DOM::Node* dom_node) : BlockBox(dom_node) {
    m_marker = std::make_unique<RenderMarker>(dom_node);
}

const Rect& RenderListItem::marker_rect() const {
    return m_marker ? m_marker->get_rect() : m_rect;
}

void RenderListItem::layout(IGraphicsContext& context, const Rect& bounds) {
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

    float marker_offset = kMarkerSize + kMarkerGap;
    float content_width = m_rect.width - inset_left - inset_right - marker_offset;
    if (content_width < 0.0f) content_width = 0.0f;

    float cursor_x = inset_left + marker_offset;
    float cursor_y = inset_top;
    float line_height = 0.0f;
    float marker_y = inset_top;
    bool marker_y_set = false;

    auto flush_line = [&]() {
        cursor_y += line_height;
        cursor_x = inset_left + marker_offset;
        line_height = 0.0f;
    };

    size_t i = 0;
    while (i < m_children.size()) {
        auto& child = m_children[i];
        const auto* child_style = child->get_computed_style();
        float margin_left = child_style ? child_style->margin.left : 0.0f;
        float margin_right = child_style ? child_style->margin.right : 0.0f;
        float margin_top = child_style ? child_style->margin.top : 0.0f;
        float margin_bottom = child_style ? child_style->margin.bottom : 0.0f;

        bool is_inline = child->is_inline();

        if (!is_inline) {
            if (child_style && margin_top > 0.0f) {
                cursor_y += margin_top;
            }
            flush_line();
            float child_x = inset_left + marker_offset + margin_left;
            float child_y = cursor_y;
            float available_width = content_width - margin_left - margin_right;
            Rect child_bounds = {child_x, child_y, available_width, 0.0f};
            child->layout(context, child_bounds);
            cursor_y = child_y + child->get_rect().height + margin_bottom;
            if (!marker_y_set) {
                marker_y = inset_top;
                marker_y_set = true;
            }
            ++i;
            continue;
        }

        InlineLineBuilder builder;
        builder.reset();
        std::vector<InlineRun> runs;
        size_t group_start = i;

        while (i < m_children.size() && m_children[i]->is_inline()) {
            auto& inline_child = m_children[i];
            inline_child->reset_inline_layout();
            inline_child->collect_inline_runs(context, runs);
            ++i;
        }

        if (runs.empty()) {
            continue;
        }

        for (const auto& run : runs) {
            builder.add_run(run);
        }

        float start_x = cursor_x - (inset_left + marker_offset);
        auto fragments = builder.layout(content_width, start_x);
        float base_x = inset_left + marker_offset;
        float base_y = cursor_y;

        for (auto& fragment : fragments) {
            fragment.rect.x += base_x;
            fragment.rect.y += base_y;
            auto& run = runs[fragment.run_index];
            if (run.owner) {
                run.owner->apply_inline_fragment(run.local_index, fragment, run);
            }
        }

        for (size_t j = group_start; j < i; ++j) {
            m_children[j]->finalize_inline_layout();
        }

        const auto& heights = builder.line_heights();
        if (!heights.empty()) {
            float total_height = 0.0f;
            for (float h : heights) total_height += h;
            float last_height = heights.back();
            size_t last_line = heights.size() - 1;
            float last_line_width = 0.0f;
            for (const auto& fragment : fragments) {
                if (fragment.line_index == last_line) {
                    last_line_width = std::max(last_line_width, fragment.rect.x + fragment.rect.width - base_x);
                }
            }
            if (!marker_y_set) {
                marker_y = inset_top + std::max(0.0f, (heights[0] - kMarkerSize) * 0.5f);
                marker_y_set = true;
            }
            cursor_y = base_y + (total_height - last_height);
            cursor_x = inset_left + marker_offset + last_line_width;
            line_height = std::max(line_height, last_height);
        }
    }

    flush_line();
    m_rect.height = cursor_y + inset_bottom;

    if (m_marker) {
        Rect marker_bounds{inset_left, marker_y, kMarkerSize, kMarkerSize};
        m_marker->layout(context, marker_bounds);
    }
}

void RenderListItem::paint_self(IGraphicsContext& context, const Point& offset) {
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

void RenderMarker::paint(IGraphicsContext& context, const Point& offset) {
    const auto* style = get_computed_style();
    Color color = style ? style->color : Color{0, 0, 0, 255};
    Rect absolute{offset.x + m_rect.x, offset.y + m_rect.y, m_rect.width, m_rect.height};
    context.fill_rect(absolute, color);
}

}  // namespace Hummingbird::Layout
