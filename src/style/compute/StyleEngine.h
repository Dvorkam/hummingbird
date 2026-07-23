#pragma once

#include "style/compute/FontFaceRegistry.h"
#include "style/compute/Stylesheet.h"
#include "style/types/ComputedStyle.h"

namespace Hummingbird {
namespace Core::Utils {
class CompatibilityWarnings;
}
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
    explicit StyleEngine(Core::Utils::CompatibilityWarnings* compatibility_warnings = nullptr)
        : compatibility_warnings_(compatibility_warnings) {}

    // `media` supplies the viewport for @media rule evaluation; with the zero
    // default, rules behind min-* conditions do not apply (headless/test mode).
    // `fonts`, when non-null, resolves each element's font-family against the
    // registered @font-face families and stamps ComputedStyle::font_src.
    void apply(const Stylesheet& sheet, DOM::Node* root, const MediaContext& media = {},
               const FontFaceRegistry* fonts = nullptr);

private:
    Core::Utils::CompatibilityWarnings* compatibility_warnings_ = nullptr;
};

}  // namespace Hummingbird::Css
