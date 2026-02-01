#include "layout/BlockBox.h"

#include <algorithm>
#include <optional>
#include <utility>
#include <vector>

#include "layout/FloatLayoutUtils.h"
#include "layout/LayoutMetricsUtils.h"
#include "layout/PositioningUtils.h"
#include "layout/inline/InlineLayoutUtils.h"
#include "layout/inline/InlineRef.h"
#include "layout/inline/InlineTypes.h"
#include "style/ComputedStyle.h"

namespace Hummingbird {
class IGraphicsContext;
}  // namespace Hummingbird

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

Css::ComputedStyle::Float resolve_float_type(RenderObject& child) {
    const auto* style = child.get_computed_style();
    if (Positioning::is_absolute(style)) {
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

void flush_line(LineCursor& cursor, float inset_left) {
    cursor.y += cursor.line_height;
    cursor.x = inset_left;
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
        child_x = content_left + left_margin;
        child.set_rect({child_x, child.get_rect().y, child.get_rect().width, child.get_rect().height});
    }
    cursor.y = child_y + child.get_rect().height + margins.bottom;
}

void layout_float_child(IGraphicsContext& context, RenderObject& child, const ChildMargins& margins,
                        const Metrics::BoxMetrics& metrics, LineCursor& cursor, Css::ComputedStyle::Float float_type,
                        std::vector<FloatLayout::FloatBox>& floats, float& max_float_bottom) {
    flush_line(cursor, metrics.insets.left);

    float available_width = metrics.content_width - margins.left - margins.right;
    if (available_width < 0.0f) {
        available_width = 0.0f;
    }
    Rect child_bounds = {metrics.insets.left, cursor.y, available_width, 0.0f};
    child.layout(context, child_bounds);

    float content_left = metrics.insets.left;
    float content_right = metrics.insets.left + metrics.content_width;
    FloatLayout::FloatPlacement placement =
        FloatLayout::place_float(floats, float_type, cursor.y, child.get_rect().width, child.get_rect().height,
                                 margins.left, margins.right, margins.top, margins.bottom, content_left, content_right);
    child.set_rect(placement.rect);
    floats.push_back({placement.margin_rect, float_type});
    max_float_bottom = std::max(max_float_bottom, placement.margin_rect.y + placement.margin_rect.height);
}

void layout_inline_group(IGraphicsContext& context, std::vector<std::unique_ptr<RenderObject>>& children, size_t& i,
                         const Metrics::BoxMetrics& metrics, LineCursor& cursor, float base_x, float content_width,
                         Css::ComputedStyle::TextAlign text_align, float wrap_width) {
    InlineLayout::GroupLayoutContext layout_context;
    layout_context.start_x = cursor.x - base_x;
    layout_context.base_x = base_x;
    layout_context.base_y = cursor.y;
    layout_context.content_width = content_width;
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
        ChildMargins margins = compute_child_margins(child_style);

        Css::ComputedStyle::Float float_type = resolve_float_type(*child);
        if (float_type != Css::ComputedStyle::Float::None) {
            layout_float_child(context, *child, margins, metrics, cursor, float_type, floats, max_float_bottom);
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
            layout_block_child(context, *child, margins, cursor, content_left, content_width);
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
        cursor.x = std::max(cursor.x, band.left);
        layout_inline_group(context, m_children, i, metrics, cursor, band.left, band.right - band.left, align,
                            wrap_width);
    }

    flush_line(cursor, metrics.insets.left);
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
