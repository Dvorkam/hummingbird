#include "layout/flow/TextDecorationUtils.h"

#include <algorithm>

namespace Hummingbird::Layout {
namespace {
constexpr float kUnderlineOffsetPx = 2.0f;
constexpr float kUnderlineThicknessPx = 1.0f;
}  // namespace

UnderlineMetrics resolve_underline_metrics(const TextMetrics& metrics, const Css::ComputedStyle* style) {
    UnderlineMetrics underline;
    float position = -metrics.underline_position;
    float fallback = metrics.descent > 0.0f ? std::max(1.0f, metrics.descent * 0.25f) : kUnderlineOffsetPx;
    if (position <= 0.0f) {
        position = fallback;
    }
    if (metrics.descent > 0.0f) {
        position = std::min(position, metrics.descent);
    }
    if (style && style->underline_offset.has_value()) {
        position = std::max(0.0f, *style->underline_offset);
    }
    underline.position = position;
    underline.thickness = metrics.underline_thickness > 0.0f ? metrics.underline_thickness : kUnderlineThicknessPx;
    if (style && style->underline_thickness.has_value()) {
        underline.thickness = std::max(kUnderlineThicknessPx, *style->underline_thickness);
    }
    if (underline.thickness < kUnderlineThicknessPx) {
        underline.thickness = kUnderlineThicknessPx;
    }
    return underline;
}

float compute_underline_y(float line_top, float line_height, const TextMetrics& metrics,
                          const UnderlineMetrics& underline) {
    if (metrics.ascent > 0.0f) {
        return line_top + metrics.ascent + underline.position;
    }
    return line_top + line_height - kUnderlineOffsetPx;
}

}  // namespace Hummingbird::Layout
