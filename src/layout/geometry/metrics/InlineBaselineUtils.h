#pragma once

#include <algorithm>

#include "core/dom/Element.h"
#include "core/platform_api/IGraphicsContext.h"
#include "html/HtmlTagNames.h"
#include "layout/geometry/metrics/LayoutMetricsUtils.h"
#include "layout/geometry/metrics/TextMetricsUtils.h"
#include "style/types/ComputedStyle.h"

namespace Hummingbird::Layout::InlineBaselineUtils {

inline float estimate_text_ascent(IGraphicsContext& context, const Css::ComputedStyle* style) {
    return TextMetricsUtils::resolve_text_ascent(context, style);
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
