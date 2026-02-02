#pragma once

#include <algorithm>
#include <optional>
#include <string_view>

#include "core/dom/Element.h"
#include "core/utils/ParseUtils.h"
#include "html/HtmlAttributeNames.h"
#include "layout/geometry/metrics/LayoutMetricsUtils.h"
#include "style/compute/ComputedStyle.h"

namespace Hummingbird::Layout::ReplacedElementUtils {

struct LayoutSize {
    float width = 0.0f;
    float height = 0.0f;
};

struct SizeOptions {
    float default_width = 0.0f;
    float default_height = 0.0f;
    std::optional<float> intrinsic_width;
    std::optional<float> intrinsic_height;
};

inline std::optional<float> find_attribute_dimension(const DOM::Element& element, std::string_view name) {
    if (const auto value = DOM::find_attribute_value(element, name)) {
        return Core::Utils::parse_float(*value, Core::Utils::NumberParseMode::AllowTrailing);
    }
    return std::nullopt;
}

inline float resolve_dimension(const DOM::Element& element, const Css::ComputedStyle* style, std::string_view name,
                               float default_value, std::optional<float> intrinsic_value,
                               const std::optional<float>& style_value) {
    if (style && style_value.has_value()) {
        return std::max(0.0f, *style_value);
    }
    if (auto attr = find_attribute_dimension(element, name)) {
        return std::max(0.0f, *attr);
    }
    if (intrinsic_value.has_value()) {
        return std::max(0.0f, *intrinsic_value);
    }
    return default_value;
}

inline LayoutSize compute_layout_size(const DOM::Element& element, const Css::ComputedStyle* style,
                                      const SizeOptions& options) {
    Metrics::Insets insets = Metrics::compute_insets(style);
    float content_width =
        resolve_dimension(element, style, Hummingbird::Html::AttributeNames::Width, options.default_width,
                          options.intrinsic_width, style ? style->width : std::optional<float>{});
    float content_height =
        resolve_dimension(element, style, Hummingbird::Html::AttributeNames::Height, options.default_height,
                          options.intrinsic_height, style ? style->height : std::optional<float>{});

    float total_width = content_width + insets.left + insets.right;
    float total_height = content_height + insets.top + insets.bottom;

    if (style && style->box_sizing == Css::ComputedStyle::BoxSizing::BorderBox) {
        if (style->width.has_value()) {
            total_width = std::max(0.0f, *style->width);
            content_width = Metrics::content_width(total_width, insets);
        }
        if (style->height.has_value()) {
            total_height = std::max(0.0f, *style->height);
            content_height = std::max(0.0f, total_height - insets.top - insets.bottom);
        }
    }

    if (style) {
        if (style->min_width.has_value()) {
            total_width = std::max(total_width, Metrics::resolve_border_box_width(style, *style->min_width, insets));
        }
        if (style->max_width.has_value()) {
            total_width = std::min(total_width, Metrics::resolve_border_box_width(style, *style->max_width, insets));
        }
        if (style->min_height.has_value()) {
            total_height =
                std::max(total_height, Metrics::resolve_border_box_height(style, *style->min_height, insets));
        }
        if (style->max_height.has_value()) {
            total_height =
                std::min(total_height, Metrics::resolve_border_box_height(style, *style->max_height, insets));
        }
    }

    return {total_width, total_height};
}

}  // namespace Hummingbird::Layout::ReplacedElementUtils
