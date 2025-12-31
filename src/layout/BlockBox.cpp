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

InlineRun build_inline_atomic_run(InlineBlockBox& box, IGraphicsContext& context) {
    box.layout(context, {0.0f, 0.0f, kInlineAtomicLayoutWidth, 0.0f});
    const auto& rect = box.get_rect();
    InlineRun run;
    run.owner = &box;
    run.local_index = 0;
    run.width = rect.width;
    run.height = rect.height;
    return run;
}

void collect_inline_runs(IGraphicsContext& context, std::vector<std::unique_ptr<RenderObject>>& children, size_t& i,
                         std::vector<InlineRun>& runs) {
    while (i < children.size()) {
        auto inl = children[i]->Inline();
        if (!inl) break;

        inl.get().reset_inline_layout();
        inl.get().collect_inline_runs(context, runs);
        ++i;
    }
}

void layout_inline_group(IGraphicsContext& context, std::vector<std::unique_ptr<RenderObject>>& children, size_t& i,
                         const LayoutMetrics& metrics, LineCursor& cursor) {
    InlineLineBuilder builder;
    builder.reset();
    std::vector<InlineRun> runs;
    size_t group_start = i;

    collect_inline_runs(context, children, i, runs);

    if (runs.empty()) {
        return;
    }

    for (const auto& run : runs) {
        builder.add_run(run);
    }

    float start_x = cursor.x - metrics.inset_left;
    auto lines = builder.layout(metrics.content_width, start_x);
    float base_x = metrics.inset_left;
    float base_y = cursor.y;

    if (lines.empty()) {
        return;
    }

    std::vector<InlineFragment> fragments;
    fragments.reserve(runs.size());
    std::vector<float> heights;
    heights.reserve(lines.size());
    for (const auto& line : lines) {
        heights.push_back(line.height);
        for (const auto& fragment : line.fragments) {
            fragments.push_back(fragment);
        }
    }

    for (auto& fragment : fragments) {
        fragment.rect.x += base_x;
        fragment.rect.y += base_y;
        auto& run = runs[fragment.run_index];
        if (run.owner) {
            if (auto inl = run.owner->Inline()) {
                inl.get().apply_inline_fragment(run.local_index, fragment, run);
            }
        }
    }

    for (size_t j = group_start; j < i; ++j) {
        if (auto inl = children[j]->Inline()) {
            inl.get().finalize_inline_layout();
        }
    }

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
    cursor.y = base_y + (total_height - last_height);
    cursor.x = metrics.inset_left + last_line_width;
    cursor.line_height = std::max(cursor.line_height, last_height);
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
        layout_inline_group(context, m_children, i, metrics, cursor);
    }

    flush_line(cursor, metrics.inset_left);
    m_rect.height = cursor.y + metrics.inset_bottom;
}

void InlineBlockBox::reset_inline_layout() {
    m_inline_atomic = false;
}

void InlineBlockBox::collect_inline_runs(IGraphicsContext& context, std::vector<InlineRun>& runs) {
    m_inline_atomic = true;
    runs.push_back(build_inline_atomic_run(*this, context));
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
