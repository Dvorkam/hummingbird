#pragma once

#include "style/compute/ComputedStyle.h"
#include "style/compute/Stylesheet.h"

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
    void apply(const Stylesheet& sheet, DOM::Node* root);

private:
    void compute_node(const Stylesheet& sheet, DOM::Node* node, const ComputedStyle* parent_style);
};

}  // namespace Hummingbird::Css
