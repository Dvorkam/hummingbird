#include "layout/BlockBox.h"

#include <algorithm>

#include "layout/InlineLineBuilder.h"

namespace Hummingbird::Layout {

namespace {
struct LayoutMetrics {
    float inset_left;
    float inset_right;
    float inset_top;
    float inset_bottom;
    float content_width;
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

struct InlineLayoutMetrics {
    std::vector<float> heights;
    float last_line_width = 0.0f;
};

constexpr float kInlineAtomicLayoutWidth = 100000.0f;

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

    float content_width = rect.width - inset_left - inset_right;
    return {inset_left, inset_right, inset_top, inset_bottom, content_width};
}

ChildMargins compute_child_margins(const Css::ComputedStyle* style) {
    return {style ? style->margin.left : 0.0f, style ? style->margin.right : 0.0f, style ? style->margin.top : 0.0f,
            style ? style->margin.bottom : 0.0f};
}

void flush_line(LineCursor& cursor, float inset_left) {
    cursor.y += cursor.line_height;
    cursor.x = inset_left;
    cursor.line_height = 0.0f;
}

void layout_block_child(IGraphicsContext& context, RenderObject& child, const ChildMargins& margins,
                        const LayoutMetrics& metrics, LineCursor& cursor) {
    if (margins.top > 0.0f) {
        cursor.y += margins.top;
    }
    flush_line(cursor, metrics.inset_left);
    float child_x = metrics.inset_left + margins.left;
    float child_y = cursor.y;
    float available_width = metrics.content_width - margins.left - margins.right;
    Rect child_bounds = {child_x, child_y, available_width, 0.0f};
    child.layout(context, child_bounds);
    cursor.y = child_y + child.get_rect().height + margins.bottom;
}

void measure_inline_participants(IGraphicsContext& context, std::vector<std::unique_ptr<RenderObject>>& children,
                                 size_t& i) {
    while (i < children.size()) {
        auto inl = children[i]->Inline();
        if (!inl) break;

        inl.get().reset_inline_layout();
        inl.get().measure_inline(context);
        ++i;
    }
}

void collect_inline_runs(IGraphicsContext& context, std::vector<std::unique_ptr<RenderObject>>& children, size_t& i,
                         std::vector<InlineRun>& runs) {
    while (i < children.size()) {
        auto inl = children[i]->Inline();
        if (!inl) break;

        inl.get().collect_inline_runs(context, runs);
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

InlineLayoutMetrics apply_inline_fragments(const std::vector<InlineLine>& lines, const std::vector<InlineRun>& runs,
                                           float base_x, float base_y) {
    InlineLayoutMetrics metrics;
    if (lines.empty()) {
        return metrics;
    }

    metrics.heights.reserve(lines.size());
    size_t last_line = lines.size() - 1;

    for (const auto& line : lines) {
        metrics.heights.push_back(line.height);
        for (const auto& fragment : line.fragments) {
            InlineFragment resolved = fragment;
            resolved.rect.x += base_x;
            resolved.rect.y += base_y;
            auto& run = runs[resolved.run_index];
            if (run.owner) {
                if (auto inl = run.owner->Inline()) {
                    inl.get().apply_inline_fragment(run.local_index, resolved, run);
                }
            }
            if (resolved.line_index == last_line) {
                float extent = resolved.rect.x + resolved.rect.width - base_x;
                metrics.last_line_width = std::max(metrics.last_line_width, extent);
            }
        }
    }

    return metrics;
}

void update_cursor_for_inline(LineCursor& cursor, const LayoutMetrics& metrics, float base_y,
                              const InlineLayoutMetrics& layout) {
    if (layout.heights.empty()) {
        return;
    }

    float total_height = 0.0f;
    for (float h : layout.heights) {
        total_height += h;
    }
    float last_height = layout.heights.back();
    cursor.y = base_y + (total_height - last_height);
    cursor.x = metrics.inset_left + layout.last_line_width;
    cursor.line_height = std::max(cursor.line_height, last_height);
}

void layout_inline_group(IGraphicsContext& context, std::vector<std::unique_ptr<RenderObject>>& children, size_t& i,
                         const LayoutMetrics& metrics, LineCursor& cursor, Css::ComputedStyle::TextAlign text_align,
                         float wrap_width) {
    InlineLineBuilder builder;
    builder.reset();
    std::vector<InlineRun> runs;
    size_t group_start = i;
    size_t group_end = i;

    measure_inline_participants(context, children, group_end);
    i = group_start;
    collect_inline_runs(context, children, i, runs);

    if (runs.empty()) {
        return;
    }

    for (const auto& run : runs) {
        builder.add_run(run);
    }

    float start_x = cursor.x - metrics.inset_left;
    auto lines = builder.layout(wrap_width, start_x);
    align_inline_lines(lines, metrics.content_width, text_align);
    float base_x = metrics.inset_left;
    float base_y = cursor.y;

    InlineLayoutMetrics layout = apply_inline_fragments(lines, runs, base_x, base_y);

    for (size_t j = group_start; j < group_end; ++j) {
        if (auto inl = children[j]->Inline()) {
            inl.get().finalize_inline_layout();
        }
    }

    update_cursor_for_inline(cursor, metrics, base_y, layout);
}
}  // namespace

void BlockBox::layout(IGraphicsContext& context, const Rect& bounds) {
    const auto* style = get_computed_style();
    LayoutMetrics metrics = compute_metrics(style, bounds, m_rect);
    LineCursor cursor{metrics.inset_left, metrics.inset_top, 0.0f};

    size_t i = 0;
    while (i < m_children.size()) {
        auto& child = m_children[i];
        const auto* child_style = child->get_computed_style();
        ChildMargins margins = compute_child_margins(child_style);

        if (!child->Inline()) {
            // Control objects like <br> need to break the line before stacking blocks.
            layout_block_child(context, *child, margins, metrics, cursor);
            ++i;
            continue;
        }
        auto align = style ? style->text_align : Css::ComputedStyle::TextAlign::Left;
        float wrap_width = (style && style->whitespace == Css::ComputedStyle::WhiteSpace::NoWrap)
                               ? 0.0f
                               : metrics.content_width;
        layout_inline_group(context, m_children, i, metrics, cursor, align, wrap_width);
    }

    flush_line(cursor, metrics.inset_left);
    m_rect.height = cursor.y + metrics.inset_bottom;
}

void InlineBlockBox::reset_inline_layout() {
    m_inline_atomic = false;
    m_inline_measured_width = 0.0f;
    m_inline_measured_height = 0.0f;
}

void InlineBlockBox::measure_inline(IGraphicsContext& context) {
    m_inline_atomic = true;
    layout(context, {0.0f, 0.0f, kInlineAtomicLayoutWidth, 0.0f});
    m_inline_measured_width = m_rect.width;
    m_inline_measured_height = m_rect.height;
}

void InlineBlockBox::collect_inline_runs(IGraphicsContext& /*context*/, std::vector<InlineRun>& runs) {
    InlineRun run;
    run.owner = this;
    run.local_index = 0;
    run.width = m_inline_measured_width;
    run.height = m_inline_measured_height;
    runs.push_back(std::move(run));
}

void InlineBlockBox::apply_inline_fragment(size_t index, const InlineFragment& fragment, const InlineRun& run) {
    if (!m_inline_atomic || index != 0) {
        return;
    }
    m_rect.x = fragment.rect.x;
    m_rect.y = fragment.rect.y;
    m_rect.width = run.width;
    m_rect.height = run.height;
}

void InlineBlockBox::finalize_inline_layout() {
    if (!m_inline_atomic) {
        m_rect = {};
    }
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
