#pragma once

#include "style/compute/Stylesheet.h"

namespace Hummingbird {
namespace Css {
struct Selector;
}  // namespace Css
}  // namespace Hummingbird

namespace Hummingbird::DOM {
class Node;
}

namespace Hummingbird::Css {

bool matches_selector(const DOM::Node* node, const Selector& selector);

}  // namespace Hummingbird::Css
