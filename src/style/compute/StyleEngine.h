#pragma once

#include "style/compute/Stylesheet.h"
#include "style/types/ComputedStyle.h"

namespace Hummingbird {
namespace Css {
struct ComputedStyle;
struct Stylesheet;
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
    void apply(const Stylesheet& sheet, DOM::Node* root, const MediaContext& media = {});

private:
    void compute_node(const Stylesheet& sheet, DOM::Node* node, const ComputedStyle* parent_style,
                      const MediaContext& media);
};

}  // namespace Hummingbird::Css
