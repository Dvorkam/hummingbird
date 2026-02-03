#pragma once

#include <algorithm>

#include "core/platform_api/IGraphicsContext.h"
#include "layout/flow/TextStyleUtils.h"
#include "style/compute/ComputedStyle.h"

namespace Hummingbird::Layout::TextMetricsUtils {

inline float resolve_line_height(const Css::ComputedStyle* style, float fallback) {
    float line_height = fallback;
    if (style && style->line_height > 0.0f) {
        line_height = style->line_height;
    }
    return line_height;
}

inline float resolve_line_height(const Css::ComputedStyle* style, const TextMetrics& metrics) {
    return resolve_line_height(style, metrics.height);
}

inline float resolve_text_ascent(const TextMetrics& metrics, float line_height) {
    if (metrics.ascent > 0.0f) {
        float ascent = metrics.ascent;
        if (metrics.height > 0.0f && line_height > metrics.height) {
            float extra = line_height - metrics.height;
            ascent += extra * 0.5f;
        }
        if (line_height > 0.0f) {
            ascent = std::min(ascent, line_height);
        }
        return ascent;
    }
    if (line_height > 0.0f) {
        return line_height;
    }
    return metrics.height;
}

inline float resolve_text_ascent(IGraphicsContext& context, const Css::ComputedStyle* style) {
    TextStyle text_style = TextStyleUtils::build_text_style(style);
    TextMetrics metrics = context.measure_text("A", text_style);
    float line_height = resolve_line_height(style, metrics);
    return resolve_text_ascent(metrics, line_height);
}

}  // namespace Hummingbird::Layout::TextMetricsUtils
