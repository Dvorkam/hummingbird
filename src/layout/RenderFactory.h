#pragma once

#include <memory>

namespace Hummingbird::DOM {
class Node;
class Text;
}  // namespace Hummingbird::DOM

namespace Hummingbird::Layout {

class RenderObject;

class RenderFactory {
public:
    static std::unique_ptr<RenderObject> create_block_box(const DOM::Node* dom_node);
    static std::unique_ptr<RenderObject> create_inline_box(const DOM::Node* dom_node);
    static std::unique_ptr<RenderObject> create_inline_block_box(const DOM::Node* dom_node);
    static std::unique_ptr<RenderObject> create_list_item(const DOM::Node* dom_node);
    static std::unique_ptr<RenderObject> create_break(const DOM::Node* dom_node);
    static std::unique_ptr<RenderObject> create_rule(const DOM::Node* dom_node);
    static std::unique_ptr<RenderObject> create_text_box(const DOM::Text* dom_node);
};

}  // namespace Hummingbird::Layout
