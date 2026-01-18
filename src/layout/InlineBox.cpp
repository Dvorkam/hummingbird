#include "layout/InlineBox.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "layout/LayoutMetricsUtils.h"
#include "layout/inline/InlineRef.h"
#include "layout/inline/InlineTypes.h"
#include "style/ComputedStyle.h"

namespace Hummingbird::Layout {

namespace {
constexpr float kInlineAtomicLayoutWidth = 100000.0f;

struct ChildMargins {
    float left;
    float right;
    float top;
    float bottom;
};

bool has_insets(const Css::ComputedStyle* style) {
    Metrics::Insets insets = Metrics::compute_insets(style);
    return insets.left > 0.0f || insets.right > 0.0f || insets.top > 0.0f || insets.bottom > 0.0f;
}

ChildMargins compute_child_margins(const Css::ComputedStyle* style) {
    return {style ? style->margin.left : 0.0f, style ? style->margin.right : 0.0f, style ? style->margin.top : 0.0f,
            style ? style->margin.bottom : 0.0f};
}
}  // namespace

void InlineBox::reset_inline_layout() {
    m_inline_atomic = false;
    m_inline_measured_width = 0.0f;
    m_inline_measured_height = 0.0f;
}

void InlineBox::measure_inline(IGraphicsContext& context) {
    const auto* style = get_computed_style();

    if (has_insets(style)) {
        m_inline_atomic = true;
        layout(context, {0.0f, 0.0f, kInlineAtomicLayoutWidth, 0.0f});
        m_inline_measured_width = m_rect.width;
        m_inline_measured_height = m_rect.height;
        return;
    }

    m_inline_atomic = false;
    for (auto& child : m_children) {
        if (auto p = child->Inline()) {
            p.get().reset_inline_layout();
            p.get().measure_inline(context);
        }
    }
}

void InlineBox::collect_inline_runs(IGraphicsContext& context, std::vector<InlineRun>& runs) {
    if (m_inline_atomic) {
        InlineRun run;
        run.owner = this;
        run.local_index = 0;
        run.width = m_inline_measured_width;
        run.height = m_inline_measured_height;
        runs.push_back(std::move(run));
        return;
    }

    for (auto& child : m_children) {
        if (auto p = child->Inline()) {
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
    Metrics::BoxMetrics metrics = Metrics::compute_box_metrics(style, bounds, m_rect, Metrics::BoxWidthPolicy::Ignore);
    float cursor_x = metrics.insets.left;
    float cursor_y = metrics.insets.top;
    float line_height = 0.0f;

    for (auto& child : m_children) {
        const auto* child_style = child->get_computed_style();
        ChildMargins margins = compute_child_margins(child_style);

        float child_x = cursor_x + margins.left;
        float child_y = cursor_y + margins.top;
        float available_width = metrics.content_width - (child_x - metrics.insets.left);
        Rect child_bounds{child_x, child_y, available_width, 0.0f};
        child->layout(context, child_bounds);

        float child_height = child->get_rect().height + margins.top + margins.bottom;
        line_height = std::max(line_height, child_height);
        cursor_x = child_x + child->get_rect().width + margins.right;
    }

    m_rect.width = cursor_x + metrics.insets.right;
    m_rect.height = cursor_y + line_height + metrics.insets.bottom;
}

}  // namespace Hummingbird::Layout
