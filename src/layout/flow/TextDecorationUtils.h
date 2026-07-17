#pragma once

#include "core/GraphicsTypes.h"
#include "style/types/ComputedStyle.h"

namespace Hummingbird::Layout {

struct UnderlineMetrics {
    float position = 0.0f;
    float thickness = 1.0f;
};

UnderlineMetrics resolve_underline_metrics(const TextMetrics& metrics, const Css::ComputedStyle* style);
float compute_underline_y(float line_top, float line_height, const TextMetrics& metrics,
                          const UnderlineMetrics& underline);

}  // namespace Hummingbird::Layout
