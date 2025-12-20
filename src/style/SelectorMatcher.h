#pragma once

#include "style/Stylesheet.h"

namespace Hummingbird::DOM {
class Node;
}

namespace Hummingbird::Css {

bool matches_selector(const DOM::Node* node, const Selector& selector);

}  // namespace Hummingbird::Css
