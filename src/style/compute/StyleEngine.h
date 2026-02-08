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
    void apply(const Stylesheet& sheet, DOM::Node* root);

private:
    void compute_node(const Stylesheet& sheet, DOM::Node* node, const ComputedStyle* parent_style);
};

}  // namespace Hummingbird::Css
