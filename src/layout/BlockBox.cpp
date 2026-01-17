#include "layout/BlockBox.h"

#include <algorithm>

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
    bool left_auto;
    bool right_auto;
};

constexpr float kInlineAtomicLayoutWidth = 100000.0f;

ChildMargins compute_child_margins(const Css::ComputedStyle* style) {
    return {style ? style->margin.left : 0.0f,       style ? style->margin.right : 0.0f,
            style ? style->margin.top : 0.0f,        style ? style->margin.bottom : 0.0f,
            style ? style->margin_left_auto : false, style ? style->margin_right_auto : false};
}

void flush_line(LineCursor& cursor, float inset_left) {
    cursor.y += cursor.line_height;
    cursor.x = inset_left;
    cursor.line_height = 0.0f;
}

void layout_block_child(IGraphicsContext& context, RenderObject& child, const ChildMargins& margins,
                        const Metrics::BoxMetrics& metrics, LineCursor& cursor) {
    if (margins.top > 0.0f) {
        cursor.y += margins.top;
    }
    flush_line(cursor, metrics.insets.left);
    float child_x = metrics.insets.left + margins.left;
    float child_y = cursor.y;
    float available_width =
        metrics.content_width - (margins.left_auto ? 0.0f : margins.left) - (margins.right_auto ? 0.0f : margins.right);
    Rect child_bounds = {child_x, child_y, available_width, 0.0f};
    child.layout(context, child_bounds);
    if (margins.left_auto || margins.right_auto) {
        float remaining = metrics.content_width - child.get_rect().width - (margins.left_auto ? 0.0f : margins.left) -
                          (margins.right_auto ? 0.0f : margins.right);
        if (remaining < 0.0f) remaining = 0.0f;
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
        child_x = metrics.insets.left + left_margin;
        child.set_rect({child_x, child.get_rect().y, child.get_rect().width, child.get_rect().height});
    }
    cursor.y = child_y + child.get_rect().height + margins.bottom;
}

void layout_inline_group(IGraphicsContext& context, std::vector<std::unique_ptr<RenderObject>>& children, size_t& i,
                         const Metrics::BoxMetrics& metrics, LineCursor& cursor,
                         Css::ComputedStyle::TextAlign text_align, float wrap_width) {
    InlineLayout::GroupLayoutContext layout_context;
    layout_context.start_x = cursor.x - metrics.insets.left;
    layout_context.base_x = metrics.insets.left;
    layout_context.base_y = cursor.y;
    layout_context.content_width = metrics.content_width;
    layout_context.align = text_align;
    layout_context.wrap_width = wrap_width;
    layout_context.capture_fragments = false;
    InlineLayout::InlineLayoutResult layout = InlineLayout::layout_inline_group(context, children, i, layout_context);

    InlineLayout::update_cursor_for_inline(cursor.x, cursor.y, cursor.line_height, layout_context.base_x,
                                           layout_context.base_y, layout);
}
}  // namespace

void BlockBox::layout(IGraphicsContext& context, const Rect& bounds) {
    const auto* style = get_computed_style();
    Metrics::BoxMetrics metrics =
        Metrics::compute_box_metrics(style, bounds, m_rect, Metrics::BoxWidthPolicy::WidthAndMax);
    LineCursor cursor{metrics.insets.left, metrics.insets.top, 0.0f};

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
        // Avoid using text-align during intrinsic measurement with oversized widths.
        if (align != Css::ComputedStyle::TextAlign::Left && bounds.width >= kInlineAtomicLayoutWidth &&
            !(style && style->width.has_value())) {
            align = Css::ComputedStyle::TextAlign::Left;
        }
        float wrap_width =
            (style && style->whitespace == Css::ComputedStyle::WhiteSpace::NoWrap) ? 0.0f : metrics.content_width;
        layout_inline_group(context, m_children, i, metrics, cursor, align, wrap_width);
    }

    flush_line(cursor, metrics.insets.left);
    m_rect.height = cursor.y + metrics.insets.bottom;
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

    Metrics::Insets insets = Metrics::compute_insets(style);
    float inset_left = insets.left;
    float inset_right = insets.right;

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
