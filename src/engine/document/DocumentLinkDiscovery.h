#pragma once

#include <string>
#include <vector>

namespace Hummingbird::DOM {
class Node;
}

namespace Hummingbird::Engine {

std::vector<std::string> collect_script_blocks_from_dom(const DOM::Node* root);
std::vector<std::string> collect_background_image_links_from_dom(const DOM::Node* root);

}  // namespace Hummingbird::Engine
