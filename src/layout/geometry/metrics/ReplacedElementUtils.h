#pragma once

#include <algorithm>
#include <optional>
#include <string_view>

#include "core/dom/Element.h"
#include "core/dom/ElementUtils.h"
#include "core/utils/ParseUtils.h"
#include "html/HtmlAttributeNames.h"
#include "layout/geometry/metrics/LayoutMetricsUtils.h"
#include "style/types/ComputedStyle.h"

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
    // Containing-block content width/height, used as the reference length when a
    // dimension is a percentage (story 8.5.1). Absent means no reference is
    // available. Width retains the bounded inline-measure fallback documented in
    // resolve_length; height computes to auto in compute_layout_size.
    std::optional<float> containing_block_width;
    std::optional<float> containing_block_height;
};

inline std::optional<float> find_attribute_dimension(const DOM::Element& element, std::string_view name) {
    if (const auto value = DOM::find_attribute_value(element, name)) {
        return Core::Utils::parse_float(*value, Core::Utils::NumberParseMode::AllowTrailing);
    }
    return std::nullopt;
}

// A basis at or above this is an intrinsic-measurement probe, not a real
// containing block: BlockBox/FlexBox lay children out at kInlineAtomicLayoutWidth
// (100000) to measure max-content. A percentage against a probe is indefinite for
// sizing, so we must NOT resolve `width:100%` to ~100000 there — that reintroduces
// the shrink-to-fit ballooning bug (T-LAYOUT-SHRINK-TO-FIT-1) for replaced
// elements. 20000 mirrors the table code's threshold.
inline constexpr float kIntrinsicMeasureThreshold = 20000.0f;

inline bool has_definite_percentage_basis(std::optional<float> percent_basis) {
    return percent_basis.has_value() && *percent_basis < kIntrinsicMeasureThreshold;
}

// Resolve a computed length to pixels. With a definite `percent_basis` (the
// containing block's content extent) a percentage resolves against it, matching
// CSS — this is what keeps a `width:100%`/`2rem` icon from ballooning to its
// intrinsic size (story 8.5.1). With no basis, or a basis that is really an
// intrinsic-measurement probe, a percentage is taken as its bare magnitude (the
// pre-unification behavior): small and non-ballooning, so neither the inline path
// (no containing block plumbed) nor the measurement probe regresses.
inline float resolve_length(const Css::ComputedStyle::LengthValue& value, std::optional<float> percent_basis) {
    if (has_definite_percentage_basis(percent_basis)) {
        return value.resolve(*percent_basis);
    }
    return value.has_percent ? value.percent : value.px;
}

// Convenience for the no-basis call sites (min/max on the inline path etc.).
inline float raw_px(const Css::ComputedStyle::LengthValue& value) {
    return resolve_length(value, std::nullopt);
}

inline float resolve_dimension(const DOM::Element& element, const Css::ComputedStyle* style, std::string_view name,
                               float default_value, std::optional<float> intrinsic_value,
                               const std::optional<Css::ComputedStyle::LengthValue>& style_value,
                               std::optional<float> percent_basis) {
    if (style) {
        if (style_value.has_value()) {
            return std::max(0.0f, resolve_length(*style_value, percent_basis));
        }
    } else if (auto attr = find_attribute_dimension(element, name)) {
        // Unstyled render objects do not have the presentational-hint pass that
        // maps HTML width/height attributes into ComputedStyle. Keep the raw
        // attribute fallback only for that defensive path. Once a computed
        // style exists it is authoritative: re-reading an attribute here would
        // resurrect a hint that author CSS (for example height:auto) overrode.
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
    using LV = Css::ComputedStyle::LengthValue;
    // Percentages resolve against the containing block: horizontal dimensions
    // (width, min/max-width) against its content width, vertical against its
    // height. An absent horizontal basis uses the bounded inline-measure fallback;
    // an absent vertical basis makes a percentage height auto below.
    const std::optional<float> w_basis = options.containing_block_width;
    const std::optional<float> h_basis = options.containing_block_height;
    // CSS percentage heights compute to auto when the containing block's height
    // is indefinite. Seznam's picture wrapper uses width:100%; height:100% inside
    // an auto-height block; treating the token magnitude as 100px stretches every
    // image. Keep the declaration out of sizing so the intrinsic ratio can
    // determine the auto height below. A future definite-height propagation path
    // supplies h_basis and naturally takes the regular percentage branch.
    const bool height_percentage_is_auto =
        style && style->height.has_value() && style->height->has_percent && !has_definite_percentage_basis(h_basis);
    float content_width =
        resolve_dimension(element, style, Hummingbird::Html::AttributeNames::Width, options.default_width,
                          options.intrinsic_width, style ? style->width : std::optional<LV>{}, w_basis);
    float content_height =
        height_percentage_is_auto
            ? std::max(0.0f, options.intrinsic_height.value_or(options.default_height))
            : resolve_dimension(element, style, Hummingbird::Html::AttributeNames::Height, options.default_height,
                                options.intrinsic_height, style ? style->height : std::optional<LV>{}, h_basis);

    // Intrinsic aspect ratio: when exactly one dimension is specified (by CSS or
    // an HTML attribute) and the other is auto, derive the auto one from the
    // media's natural ratio instead of using its raw intrinsic size. This is why
    // `<img style="width:100%">` in a square cell stays square rather than
    // rendering full-height. Both-specified keeps both; neither-specified keeps
    // the natural size.
    const bool width_specified =
        style ? style->width.has_value()
              : find_attribute_dimension(element, Hummingbird::Html::AttributeNames::Width).has_value();
    const bool height_specified =
        style ? (style->height.has_value() && !height_percentage_is_auto)
              : find_attribute_dimension(element, Hummingbird::Html::AttributeNames::Height).has_value();
    const bool has_ratio = options.intrinsic_width.has_value() && options.intrinsic_height.has_value() &&
                           *options.intrinsic_width > 0.0f && *options.intrinsic_height > 0.0f;
    const float ratio = has_ratio ? *options.intrinsic_width / *options.intrinsic_height : 1.0f;
    if (has_ratio) {
        if (width_specified && !height_specified) {
            content_height = content_width / ratio;
        } else if (!width_specified && height_specified) {
            content_width = content_height * ratio;
        }
    }

    float total_width = content_width + insets.left + insets.right;
    float total_height = content_height + insets.top + insets.bottom;

    if (style && style->box_sizing == Css::ComputedStyle::BoxSizing::BorderBox) {
        if (style->width.has_value()) {
            total_width = std::max(0.0f, resolve_length(*style->width, w_basis));
            content_width = Metrics::content_width(total_width, insets);
        }
        if (style->height.has_value() && !height_percentage_is_auto) {
            total_height = std::max(0.0f, resolve_length(*style->height, h_basis));
            content_height = std::max(0.0f, total_height - insets.top - insets.bottom);
        }
    }

    const auto clamp_total_width = [&](float value) {
        if (style && style->min_width.has_value()) {
            value = std::max(
                value, Metrics::resolve_border_box_width(style, resolve_length(*style->min_width, w_basis), insets));
        }
        if (style && style->max_width.has_value()) {
            value = std::min(
                value, Metrics::resolve_border_box_width(style, resolve_length(*style->max_width, w_basis), insets));
        }
        return value;
    };
    const auto clamp_total_height = [&](float value) {
        if (style && style->min_height.has_value()) {
            value = std::max(
                value, Metrics::resolve_border_box_height(style, resolve_length(*style->min_height, h_basis), insets));
        }
        if (style && style->max_height.has_value()) {
            value = std::min(
                value, Metrics::resolve_border_box_height(style, resolve_length(*style->max_height, h_basis), insets));
        }
        return value;
    };

    total_width = clamp_total_width(total_width);
    total_height = clamp_total_height(total_height);

    // Re-derive the auto axis AFTER box-sizing/min-max: clamping one axis (the
    // ubiquitous `img { max-width:100% }`) must pull the other along the intrinsic
    // ratio, or a 1000x500 image in a 300px container renders 300x500 instead of
    // 300x150 (CSS2 §10.4). Only an axis the author did not specify follows, and
    // the derived value re-applies that axis's own min/max once; the doubly-
    // constrained corner cases of the spec's resolution table are out of MVP scope.
    if (has_ratio) {
        if (!height_specified) {
            const float final_content_width = Metrics::content_width(total_width, insets);
            total_height = clamp_total_height(final_content_width / ratio + insets.top + insets.bottom);
        } else if (!width_specified) {
            const float final_content_height = std::max(0.0f, total_height - insets.top - insets.bottom);
            total_width = clamp_total_width(final_content_height * ratio + insets.left + insets.right);
        }
    }

    return {total_width, total_height};
}

}  // namespace Hummingbird::Layout::ReplacedElementUtils
