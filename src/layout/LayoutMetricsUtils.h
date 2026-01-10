#pragma once

#include "style/ComputedStyle.h"

namespace Hummingbird::Layout::Metrics {

struct Insets {
    float left = 0.0f;
    float right = 0.0f;
    float top = 0.0f;
    float bottom = 0.0f;
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

}  // namespace Hummingbird::Layout::Metrics
