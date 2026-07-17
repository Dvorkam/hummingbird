#pragma once

#include "style/compute/FontFaceRegistry.h"
#include "style/compute/Stylesheet.h"
#include "style/types/ComputedStyle.h"

namespace Hummingbird {
namespace Css {
struct ComputedStyle;
struct Stylesheet;
class FontFaceRegistry;
}  // namespace Css
}  // namespace Hummingbird

namespace Hummingbird::DOM {
class Node;
}

namespace Hummingbird::Css {

class StyleEngine {
public:
    // `media` supplies the viewport for @media rule evaluation; with the zero
    // default, rules behind min-* conditions do not apply (headless/test mode).
    // `fonts`, when non-null, resolves each element's font-family against the
    // registered @font-face families and stamps ComputedStyle::font_src.
    void apply(const Stylesheet& sheet, DOM::Node* root, const MediaContext& media = {},
               const FontFaceRegistry* fonts = nullptr);
};

}  // namespace Hummingbird::Css
