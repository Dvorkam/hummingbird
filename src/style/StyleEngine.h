#pragma once

#include "style/ComputedStyle.h"
#include "style/Stylesheet.h"

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
