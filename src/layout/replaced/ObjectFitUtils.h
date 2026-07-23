#pragma once

#include <algorithm>

#include "layout/geometry/Geometry.h"
#include "style/types/ComputedStyle.h"

namespace Hummingbird::Layout::ObjectFitUtils {

struct FitResult {
    Rect dest;              // rect to draw the image into
    bool needs_clip = false;  // dest exceeds the content box -> caller must clip to it
};

// Compute where a replaced element's content should paint inside `content` given
// its intrinsic size and the CSS `object-fit` keyword (story 8.5.2). Positioning
// is centered: `object-position` defaults to `50% 50%`, and non-default positions
// are a deferred follow-up (see T-CSS-OBJECT-FIT-1). Returns the content box
// unchanged for `fill` and for degenerate (non-positive) sizes, so the default
// keeps today's stretch behavior.
inline FitResult compute_fit(Css::ComputedStyle::ObjectFit fit, const Rect& content, float intrinsic_w,
                             float intrinsic_h) {
    using OF = Css::ComputedStyle::ObjectFit;
    if (fit == OF::Fill || intrinsic_w <= 0.0f || intrinsic_h <= 0.0f || content.width <= 0.0f ||
        content.height <= 0.0f) {
        return {content, false};
    }

    const float scale_contain = std::min(content.width / intrinsic_w, content.height / intrinsic_h);
    const float scale_cover = std::max(content.width / intrinsic_w, content.height / intrinsic_h);

    float scale = 1.0f;
    switch (fit) {
        case OF::Contain:
            scale = scale_contain;
            break;
        case OF::Cover:
            scale = scale_cover;
            break;
        case OF::None:
            scale = 1.0f;  // intrinsic size, may over- or under-flow the box
            break;
        case OF::ScaleDown:
            // The smaller of `none` and `contain`: never scale up.
            scale = std::min(1.0f, scale_contain);
            break;
        case OF::Fill:
            break;  // handled above
    }

    const float w = intrinsic_w * scale;
    const float h = intrinsic_h * scale;
    Rect dest{content.x + (content.width - w) * 0.5f, content.y + (content.height - h) * 0.5f, w, h};
    const bool needs_clip = w > content.width + 0.5f || h > content.height + 0.5f;
    return {dest, needs_clip};
}

}  // namespace Hummingbird::Layout::ObjectFitUtils
