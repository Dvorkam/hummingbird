#include "layout/replaced/ReplacedSizingUtils.h"

#include "core/dom/ElementUtils.h"

namespace Hummingbird::Layout::ReplacedSizing {

ReplacedElementUtils::LayoutSize compute_layout_size(const DOM::Element& element, const Css::ComputedStyle* style,
                                                     float default_width, float default_height,
                                                     const IntrinsicSize& intrinsic, std::optional<float> cb_width,
                                                     std::optional<float> cb_height) {
    ReplacedElementUtils::SizeOptions options;
    options.default_width = default_width;
    options.default_height = default_height;
    if (intrinsic.has_width) {
        options.intrinsic_width = intrinsic.width;
    }
    if (intrinsic.has_height) {
        options.intrinsic_height = intrinsic.height;
    }
    options.containing_block_width = cb_width;
    options.containing_block_height = cb_height;
    return ReplacedElementUtils::compute_layout_size(element, style, options);
}

}  // namespace Hummingbird::Layout::ReplacedSizing
