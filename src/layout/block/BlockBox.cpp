#include "layout/block/BlockBox.h"

#include <algorithm>
#include <optional>
#include <utility>
#include <vector>

#include "layout/block/FloatLayoutUtils.h"
#include "layout/flow/FlowLayoutUtils.h"
#include "layout/flow/inline/InlineRef.h"
#include "layout/flow/inline/InlineTypes.h"
#include "layout/geometry/PositioningUtils.h"
#include "layout/geometry/metrics/InlineBaselineUtils.h"
#include "layout/geometry/metrics/LayoutMetricsUtils.h"
#include "style/types/ComputedStyle.h"

namespace Hummingbird {
class IGraphicsContext;
}  // namespace Hummingbird

namespace Hummingbird::Layout {

constexpr float kInlineAtomicLayoutWidth = 100000.0f;

void BlockBox::layout(IGraphicsContext& context, const Rect& bounds) {
    const auto* style = get_computed_style();
    Metrics::BoxMetrics metrics =
        Metrics::compute_box_metrics(style, bounds, m_rect, Metrics::BoxWidthPolicy::WidthAndMax);
    FlowLayout::LineCursor cursor{metrics.insets.left, metrics.insets.top, 0.0f};
    std::vector<FloatLayout::FloatBox> floats;
    float max_float_bottom = cursor.y;

    size_t i = 0;
    while (i < m_children.size()) {
        auto& child = m_children[i];
        const auto* child_style = child->get_computed_style();
        if (Positioning::is_absolute(child_style)) {
            ++i;
            continue;
        }
        FlowLayout::ChildMargins margins = FlowLayout::compute_child_margins(child_style, true);

        Css::ComputedStyle::Float float_type = FlowLayout::resolve_float_type(*child, true);
        if (float_type != Css::ComputedStyle::Float::None) {
            FlowLayout::layout_float_child(context, *child, margins, cursor, float_type, floats, max_float_bottom,
                                           metrics.insets.left, metrics.insets.left + metrics.content_width);
            ++i;
            continue;
        }

        if (!child->Inline()) {
            float content_left = metrics.insets.left;
            float content_width = metrics.content_width;
            if (!floats.empty()) {
                float line_height_hint = FloatLayout::kFloatLineHeightFallback;
                if (child_style) {
                    if (child_style->line_height > 0.0f) {
                        line_height_hint = child_style->line_height;
                    } else {
                        line_height_hint = child_style->font_size;
                    }
                }
                if (line_height_hint <= 0.0f) {
                    line_height_hint = FloatLayout::kFloatLineHeightFallback;
                }
                float margin_top = margins.top > 0.0f ? margins.top : 0.0f;
                float band_y = cursor.y + margin_top;
                FloatLayout::FloatBand band = FloatLayout::compute_float_band(
                    floats, band_y, line_height_hint, metrics.insets.left, metrics.insets.left + metrics.content_width);
                if (band.has_overlap && (band.right - band.left) <= 0.0f && band.clear_y > band_y) {
                    cursor.y = band.clear_y;
                    band_y = cursor.y + margin_top;
                    band = FloatLayout::compute_float_band(floats, band_y, line_height_hint, metrics.insets.left,
                                                           metrics.insets.left + metrics.content_width);
                }
                content_left = band.left;
                content_width = band.right - band.left;
            }
            FlowLayout::layout_block_child(context, *child, margins, cursor, content_left, content_width);
            ++i;
            continue;
        }
        auto align = style ? style->text_align : Css::ComputedStyle::TextAlign::Left;
        // Avoid using text-align during intrinsic measurement with oversized widths.
        if (align != Css::ComputedStyle::TextAlign::Left && bounds.width >= kInlineAtomicLayoutWidth &&
            !(style && style->width.has_value())) {
            align = Css::ComputedStyle::TextAlign::Left;
        }
        float line_height_hint =
            std::max(cursor.line_height, style ? style->font_size : FloatLayout::kFloatLineHeightFallback);
        if (line_height_hint <= 0.0f) {
            line_height_hint = FloatLayout::kFloatLineHeightFallback;
        }
        FloatLayout::FloatBand band = FloatLayout::compute_float_band(
            floats, cursor.y, line_height_hint, metrics.insets.left, metrics.insets.left + metrics.content_width);
        if (band.has_overlap && (band.right - band.left) <= 0.0f && band.clear_y > cursor.y) {
            cursor.y = band.clear_y;
            band = FloatLayout::compute_float_band(floats, cursor.y, line_height_hint, metrics.insets.left,
                                                   metrics.insets.left + metrics.content_width);
        }
        float wrap_width =
            (style && style->whitespace == Css::ComputedStyle::WhiteSpace::NoWrap) ? 0.0f : (band.right - band.left);
        bool no_wrap = style && style->whitespace == Css::ComputedStyle::WhiteSpace::NoWrap;
        bool text_overflow_ellipsis = style && style->text_overflow == Css::ComputedStyle::TextOverflow::Ellipsis;
        bool at_block_first_line =
            cursor.line_height == 0.0f && cursor.y == metrics.insets.top && cursor.x <= (band.left + 0.01f);
        if (at_block_first_line && style && style->text_indent != 0.0f) {
            cursor.x = std::clamp(cursor.x + style->text_indent, band.left, band.right);
        }
        cursor.x = std::max(cursor.x, band.left);
        FlowLayout::layout_inline_group(context, m_children, i, cursor, band.left, band.right - band.left, align,
                                        wrap_width, no_wrap, text_overflow_ellipsis, false);
    }

    FlowLayout::flush_line(cursor, metrics.insets.left);
    float content_bottom = std::max(cursor.y, max_float_bottom);
    m_rect.height = content_bottom + metrics.insets.bottom;

    if (style) {
        if (style->min_height.has_value()) {
            float target = Metrics::resolve_border_box_height(style, *style->min_height, metrics.insets);
            if (m_rect.height < target) {
                m_rect.height = target;
            }
        }
        if (style->max_height.has_value()) {
            float target = Metrics::resolve_border_box_height(style, *style->max_height, metrics.insets);
            if (m_rect.height > target) {
                m_rect.height = target;
            }
        }
    }
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

void InlineBlockBox::collect_inline_runs(IGraphicsContext& context, std::vector<InlineRun>& runs) {
    const auto* style = get_computed_style();
    Metrics::Insets insets = Metrics::compute_insets(style);
    const auto* element = dynamic_cast<const DOM::Element*>(get_dom_node());
    bool use_text_baseline = InlineBaselineUtils::needs_text_baseline(element, !m_children.empty());
    InlineRun run;
    run.owner = this;
    run.local_index = 0;
    run.width = m_inline_measured_width;
    run.height = m_inline_measured_height;
    run.ascent =
        InlineBaselineUtils::resolve_atomic_inline_ascent(context, style, insets, run.height, use_text_baseline);
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
    if (style && style->height.has_value()) {
        Metrics::Insets insets = Metrics::compute_insets(style);
        float target_height = Metrics::resolve_border_box_height(style, *style->height, insets);
        if (m_rect.height < target_height) {
            m_rect.height = target_height;
        }
    }
    if (style) {
        Metrics::Insets insets = Metrics::compute_insets(style);
        if (style->min_height.has_value()) {
            float target_height = Metrics::resolve_border_box_height(style, *style->min_height, insets);
            if (m_rect.height < target_height) {
                m_rect.height = target_height;
            }
        }
        if (style->max_height.has_value()) {
            float target_height = Metrics::resolve_border_box_height(style, *style->max_height, insets);
            if (m_rect.height > target_height) {
                m_rect.height = target_height;
            }
        }
    }
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
