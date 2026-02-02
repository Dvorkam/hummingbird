#pragma once

#include <algorithm>

#include "core/dom/Element.h"
#include "core/platform_api/IGraphicsContext.h"
#include "html/HtmlTagNames.h"
#include "layout/flow/TextStyleUtils.h"
#include "layout/geometry/metrics/LayoutMetricsUtils.h"
#include "style/compute/ComputedStyle.h"

namespace Hummingbird::Layout::InlineBaselineUtils {

inline float estimate_text_ascent(IGraphicsContext& context, const Css::ComputedStyle* style) {
    TextStyle text_style = TextStyleUtils::build_text_style(style);
    TextMetrics metrics = context.measure_text("A", text_style);
    float line_height = metrics.height;
    if (style && style->line_height > 0.0f) {
        line_height = style->line_height;
    }
    float ascent = metrics.ascent > 0.0f ? metrics.ascent : line_height;
    if (metrics.height > 0.0f && line_height > metrics.height) {
        ascent += (line_height - metrics.height) * 0.5f;
    }
    if (line_height > 0.0f) {
        ascent = std::min(ascent, line_height);
    }
    return ascent;
}

inline float resolve_atomic_inline_ascent(IGraphicsContext& context, const Css::ComputedStyle* style,
                                          const Metrics::Insets& insets, float run_height, bool has_children) {
    if (!has_children) {
        return run_height;
    }
    float ascent = estimate_text_ascent(context, style);
    return std::min(run_height, insets.top + ascent);
}

inline bool needs_text_baseline(const DOM::Element* element, bool has_children) {
    if (has_children) {
        return true;
    }
    if (!element) {
        return false;
    }
    return element->get_tag_name() == Hummingbird::Html::TagNames::Input;
}

}  // namespace Hummingbird::Layout::InlineBaselineUtils
