#pragma once

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

ReplacedElementUtils::LayoutSize compute_layout_size(const DOM::Element& element, const Css::ComputedStyle* style,
                                                     float default_width, float default_height,
                                                     const IntrinsicSize& intrinsic);

}  // namespace Hummingbird::Layout::ReplacedSizing
