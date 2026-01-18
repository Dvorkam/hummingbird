#pragma once

#include <algorithm>

#include "layout/Geometry.h"
#include "style/ComputedStyle.h"

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

inline BoxMetrics compute_box_metrics(const Css::ComputedStyle* style, const Rect& bounds, Rect& rect,
                                      BoxWidthPolicy width_policy = BoxWidthPolicy::WidthOnly,
                                      float extra_width = 0.0f) {
    Insets insets = compute_insets(style);
    float target_width = bounds.width;
    bool constrained = false;

    if (style && width_policy != BoxWidthPolicy::Ignore && style->width.has_value()) {
        target_width = std::min(target_width, *style->width);
        constrained = true;
    }
    if (style && width_policy == BoxWidthPolicy::WidthAndMax && style->max_width.has_value()) {
        target_width = std::min(target_width, *style->max_width);
        constrained = true;
    }

    rect.x = bounds.x;
    rect.y = bounds.y;
    rect.width = constrained ? target_width + insets.left + insets.right : bounds.width;

    float width = content_width(rect.width, insets, extra_width);
    return {insets, width};
}

}  // namespace Hummingbird::Layout::Metrics
