#pragma once

#include <optional>

#include "layout/geometry/metrics/ReplacedElementUtils.h"

namespace Hummingbird {
namespace DOM {
class Element;
}  // namespace DOM
namespace Css {
struct ComputedStyle;
}  // namespace Css
}  // namespace Hummingbird

namespace Hummingbird::Layout::ReplacedSizing {

struct IntrinsicSize {
    float width = 0.0f;
    float height = 0.0f;
    bool has_width = false;
    bool has_height = false;
};

// `cb_width`/`cb_height` are the containing block's content extents, used to
// resolve percentage dimensions (story 8.5.1). Pass std::nullopt from call sites
// with no containing block available (the inline-measure path), where a
// percentage falls back to its bare magnitude.
ReplacedElementUtils::LayoutSize compute_layout_size(const DOM::Element& element, const Css::ComputedStyle* style,
                                                     float default_width, float default_height,
                                                     const IntrinsicSize& intrinsic,
                                                     std::optional<float> cb_width = std::nullopt,
                                                     std::optional<float> cb_height = std::nullopt);

}  // namespace Hummingbird::Layout::ReplacedSizing
