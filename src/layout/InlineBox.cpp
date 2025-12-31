#include "layout/InlineBox.h"

#include <algorithm>

#include "core/platform_api/IGraphicsContext.h"
#include "layout/InlineLineBuilder.h"

namespace Hummingbird::Layout {

namespace {
constexpr float kInlineAtomicLayoutWidth = 100000.0f;

struct LayoutMetrics {
    float inset_left;
    float inset_right;
    float inset_top;
    float inset_bottom;
    float content_width;
};

struct ChildMargins {
    float left;
    float right;
    float top;
    float bottom;
};

LayoutMetrics compute_metrics(const Css::ComputedStyle* style, const Rect& bounds, Rect& rect) {
    rect.x = bounds.x;
    rect.y = bounds.y;

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

    return {inset_left, inset_right, inset_top, inset_bottom, content_width};
}

bool has_insets(const Css::ComputedStyle* style) {
    if (!style) return false;
    return style->padding.left > 0.0f || style->padding.right > 0.0f || style->padding.top > 0.0f ||
           style->padding.bottom > 0.0f || style->border_width.left > 0.0f || style->border_width.right > 0.0f ||
           style->border_width.top > 0.0f || style->border_width.bottom > 0.0f;
}

ChildMargins compute_child_margins(const Css::ComputedStyle* style) {
    return {style ? style->margin.left : 0.0f, style ? style->margin.right : 0.0f, style ? style->margin.top : 0.0f,
            style ? style->margin.bottom : 0.0f};
}

InlineRun build_inline_atomic_run(InlineBox& box, IGraphicsContext& context) {
    box.layout(context, {0.0f, 0.0f, kInlineAtomicLayoutWidth, 0.0f});
    const auto& rect = box.get_rect();
    InlineRun run;
    run.owner = &box;
    run.local_index = 0;
    run.width = rect.width;
    run.height = rect.height;
    return run;
}
}  // namespace

void InlineBox::reset_inline_layout() {
    m_inline_atomic = false;
}

void InlineBox::collect_inline_runs(IGraphicsContext& context, std::vector<InlineRun>& runs) {
    const auto* style = get_computed_style();

    if (has_insets(style)) {
        m_inline_atomic = true;
        runs.push_back(build_inline_atomic_run(*this, context));
        return;
    }

    m_inline_atomic = false;
    for (auto& child : m_children) {
        if (auto p = child->Inline()) {
            p.get().reset_inline_layout();
            p.get().collect_inline_runs(context, runs);
        }
    }
}

void InlineBox::apply_inline_fragment(size_t index, const InlineFragment& fragment, const InlineRun& run) {
    if (!m_inline_atomic || index != 0) {
        return;
    }
    m_rect.x = fragment.rect.x;
    m_rect.y = fragment.rect.y;
    m_rect.width = run.width;
    m_rect.height = run.height;
}

void InlineBox::finalize_inline_layout() {
    if (m_inline_atomic) {
        return;
    }

    if (m_children.empty()) {
        m_rect = {};
        return;
    }

    bool has_bounds = false;
    float min_x = 0.0f;
    float min_y = 0.0f;
    float max_x = 0.0f;
    float max_y = 0.0f;

    for (const auto& child : m_children) {
        if (auto p = child->Inline()) {
            p.get().finalize_inline_layout();
        }
        const auto& rect = child->get_rect();
        if (!has_bounds) {
            min_x = rect.x;
            min_y = rect.y;
            max_x = rect.x + rect.width;
            max_y = rect.y + rect.height;
            has_bounds = true;
            continue;
        }
        min_x = std::min(min_x, rect.x);
        min_y = std::min(min_y, rect.y);
        max_x = std::max(max_x, rect.x + rect.width);
        max_y = std::max(max_y, rect.y + rect.height);
    }

    if (!has_bounds) {
        m_rect = {};
        return;
    }

    m_rect.x = min_x;
    m_rect.y = min_y;
    m_rect.width = max_x - min_x;
    m_rect.height = max_y - min_y;

    for (auto& child : m_children) {
        if (auto p = child->Inline()) {
            p.get().offset_inline_layout(-min_x, -min_y);
        }
    }
}

void InlineBox::layout(IGraphicsContext& context, const Rect& bounds) {
    const auto* style = get_computed_style();
    LayoutMetrics metrics = compute_metrics(style, bounds, m_rect);
    float cursor_x = metrics.inset_left;
    float cursor_y = metrics.inset_top;
    float line_height = 0.0f;

    for (auto& child : m_children) {
        const auto* child_style = child->get_computed_style();
        ChildMargins margins = compute_child_margins(child_style);

        float child_x = cursor_x + margins.left;
        float child_y = cursor_y + margins.top;
        float available_width = metrics.content_width - (child_x - metrics.inset_left);
        Rect child_bounds{child_x, child_y, available_width, 0.0f};
        child->layout(context, child_bounds);

        float child_height = child->get_rect().height + margins.top + margins.bottom;
        line_height = std::max(line_height, child_height);
        cursor_x = child_x + child->get_rect().width + margins.right;
    }

    m_rect.width = cursor_x + metrics.inset_right;
    m_rect.height = cursor_y + line_height + metrics.inset_bottom;
}

}  // namespace Hummingbird::Layout
