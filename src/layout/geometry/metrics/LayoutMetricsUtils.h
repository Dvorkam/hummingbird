#pragma once

#include <algorithm>

#include "layout/RenderObject.h"
#include "layout/geometry/Geometry.h"
#include "style/types/ComputedStyle.h"

namespace Hummingbird::Layout::Metrics {

enum class BoxWidthPolicy {
    Ignore,
    WidthOnly,
    WidthAndMax,
};

struct Insets {
    float left = 0.0f;
    float right = 0.0f;
    float top = 0.0f;
    float bottom = 0.0f;
};

struct BoxMetrics {
    Insets insets;
    float content_width = 0.0f;
};

inline Insets compute_insets(const Css::ComputedStyle* style) {
    float padding_left = style ? style->padding.left : 0.0f;
    float padding_right = style ? style->padding.right : 0.0f;
    float padding_top = style ? style->padding.top : 0.0f;
    float padding_bottom = style ? style->padding.bottom : 0.0f;
    float border_left = style ? style->border_width.left : 0.0f;
    float border_right = style ? style->border_width.right : 0.0f;
    float border_top = style ? style->border_width.top : 0.0f;
    float border_bottom = style ? style->border_width.bottom : 0.0f;
    return {padding_left + border_left, padding_right + border_right, padding_top + border_top,
            padding_bottom + border_bottom};
}

inline float content_width(float total_width, const Insets& insets, float extra = 0.0f) {
    float width = total_width - insets.left - insets.right - extra;
    if (width < 0.0f) {
        width = 0.0f;
    }
    return width;
}

inline float compute_available_width(const Rect& bounds, const Insets& insets) {
    float available = bounds.width - insets.left - insets.right;
    if (available < 0.0f) {
        available = 0.0f;
    }
    return available;
}

inline float resolve_axis_length(const Css::ComputedStyle::LengthValue& length, float reference) {
    return length.resolve(reference);
}

inline float compute_available_width(const Css::ComputedStyle* style, const Rect& bounds, const Insets& insets) {
    float available = compute_available_width(bounds, insets);
    if (style && style->width.has_value()) {
        float resolved = resolve_axis_length(*style->width, bounds.width);
        available = resolved - insets.left - insets.right;
        if (available < 0.0f) {
            available = 0.0f;
        }
    }
    return available;
}

inline float resolve_border_box_width(const Css::ComputedStyle* style, float width, const Insets& insets) {
    if (style && style->box_sizing == Css::ComputedStyle::BoxSizing::BorderBox) {
        return width;
    }
    return width + insets.left + insets.right;
}

inline float resolve_border_box_height(const Css::ComputedStyle* style, float height, const Insets& insets) {
    if (style && style->box_sizing == Css::ComputedStyle::BoxSizing::BorderBox) {
        return height;
    }
    return height + insets.top + insets.bottom;
}

// Max-content (preferred) width of `box`, which has already been laid out at an
// oversized measurement width (e.g. RenderTableCell::measure_intrinsic_width,
// FlexBox's row-direction basis probe, InlineBlockBox::measure_inline) so inline
// content sits at its natural, unwrapped positions. Reading a child's rendered
// rect width is wrong for a block-level child: a block stretches to fill the
// measurement box, which balloons the measured width (T-LAYOUT-TABLE-INTRINSIC-
// BLOCK-1, T-LAYOUT-SHRINK-TO-FIT-1). Inline-level boxes (inline, inline-block,
// replaced, text) are already sized to their content, so their width is
// trusted; only block-level boxes are derived from their content — the
// furthest child margin-box right edge, so an empty block collapses to its own
// insets. The furthest-right rule captures both formatting contexts because the
// measurement layout encodes them in the child x positions: inline siblings
// advance rightward (max = end of line = their sum), block siblings stack at
// the same left edge (max = the widest one).
inline float max_content_width(RenderObject& box) {
    const auto* style = box.get_computed_style();
    // An explicit, non-percentage width is authoritative; the box was laid out to it.
    if (style && style->width.has_value() && !style->width->has_percent) {
        return box.get_rect().width;
    }
    // Inline-level content does not stretch at the measurement width — trust it.
    if (static_cast<bool>(box.Inline())) {
        return box.get_rect().width;
    }
    // Block-level: it stretched to fill, so size it from its content instead.
    Insets insets = compute_insets(style);
    float content_right = insets.left;
    for (const auto& child : box.get_children()) {
        const auto* child_style = child->get_computed_style();
        float margin_right = child_style ? child_style->margin.right : 0.0f;
        content_right = std::max(content_right, child->get_rect().x + max_content_width(*child) + margin_right);
    }
    return content_right + insets.right;
}

inline BoxMetrics compute_box_metrics(const Css::ComputedStyle* style, const Rect& bounds, Rect& rect,
                                      BoxWidthPolicy width_policy = BoxWidthPolicy::WidthOnly,
                                      float extra_width = 0.0f) {
    Insets insets = compute_insets(style);
    float target_width = bounds.width;
    bool constrained = false;

    if (style && width_policy != BoxWidthPolicy::Ignore && style->width.has_value()) {
        float resolved = resolve_axis_length(*style->width, bounds.width);
        target_width = std::min(target_width, resolved);
        constrained = true;
    }
    if (style && width_policy != BoxWidthPolicy::Ignore && style->min_width.has_value()) {
        float resolved = resolve_axis_length(*style->min_width, bounds.width);
        target_width = std::max(target_width, resolved);
        constrained = true;
    }
    if (style && width_policy == BoxWidthPolicy::WidthAndMax && style->max_width.has_value()) {
        float resolved = resolve_axis_length(*style->max_width, bounds.width);
        target_width = std::min(target_width, resolved);
        constrained = true;
    }

    rect.x = bounds.x;
    rect.y = bounds.y;
    rect.width = constrained ? resolve_border_box_width(style, target_width, insets) : bounds.width;

    float width = content_width(rect.width, insets, extra_width);
    return {insets, width};
}

}  // namespace Hummingbird::Layout::Metrics
