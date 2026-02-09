#pragma once

#include "layout/flow/inline/InlineTypes.h"
#include "style/types/ComputedStyle.h"

namespace Hummingbird::Layout {

inline InlineVerticalAlign resolve_inline_vertical_align(const Css::ComputedStyle* style) {
    if (!style) {
        return InlineVerticalAlign::Baseline;
    }
    switch (style->vertical_align) {
        case Css::ComputedStyle::VerticalAlign::Top:
            return InlineVerticalAlign::Top;
        case Css::ComputedStyle::VerticalAlign::Middle:
            return InlineVerticalAlign::Middle;
        case Css::ComputedStyle::VerticalAlign::Bottom:
            return InlineVerticalAlign::Bottom;
        case Css::ComputedStyle::VerticalAlign::Baseline:
        default:
            return InlineVerticalAlign::Baseline;
    }
}

}  // namespace Hummingbird::Layout
