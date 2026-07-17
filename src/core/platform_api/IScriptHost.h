#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace Hummingbird::DOM {
class Element;
class Node;
}  // namespace Hummingbird::DOM

namespace Hummingbird {

// How a DOM node presents to the JS binding layer (mirrors the subset of the
// DOM `nodeType` constants Milestone 7 needs).
enum class NodeKind {
    Element,
    Text,
    Other,
};

// Boundary the JS engine (platform adapter) calls into to read and mutate the
// live document. The adapter only ever holds opaque DOM::Node*/Element* handles;
// every tree operation and every ownership decision lives behind this port.
class IScriptHost {
public:
    virtual ~IScriptHost() = default;

    // --- Lookup + content (M6) ---
    virtual DOM::Element* get_element_by_id(std::string_view id) = 0;
    virtual std::string get_text_content(const DOM::Node* node) = 0;
    virtual void set_text_content(DOM::Node* node, std::string_view text) = 0;
    virtual void set_attribute(DOM::Node* node, std::string_view name, std::string_view value) = 0;

    // --- Node factories (7.1.1) ---
    // Created nodes are detached (no parent) and owned by the host until they
    // are inserted into the tree.
    virtual DOM::Element* create_element(std::string_view tag_name) = 0;
    virtual DOM::Node* create_text_node(std::string_view data) = 0;

    // --- Tree mutation (7.1.1) ---
    // Each returns the affected child on success (the inserted node, or the
    // removed/replaced node) or nullptr when the operation is rejected
    // (missing arena, hierarchy violation, wrong parent, etc.).
    virtual DOM::Node* append_child(DOM::Node* parent, DOM::Node* child) = 0;
    virtual DOM::Node* insert_before(DOM::Node* parent, DOM::Node* child, DOM::Node* reference) = 0;
    virtual DOM::Node* remove_child(DOM::Node* parent, DOM::Node* child) = 0;
    virtual DOM::Node* replace_child(DOM::Node* parent, DOM::Node* new_child, DOM::Node* old_child) = 0;

    // --- Traversal accessors (7.1.1) ---
    virtual DOM::Node* parent_node(DOM::Node* node) = 0;
    virtual DOM::Node* first_child(DOM::Node* node) = 0;
    virtual DOM::Node* last_child(DOM::Node* node) = 0;
    virtual DOM::Node* next_sibling(DOM::Node* node) = 0;
    virtual DOM::Node* previous_sibling(DOM::Node* node) = 0;
    virtual DOM::Node* next_element_sibling(DOM::Node* node) = 0;
    virtual DOM::Node* previous_element_sibling(DOM::Node* node) = 0;
    virtual std::vector<DOM::Node*> child_nodes(DOM::Node* node) = 0;
    virtual std::vector<DOM::Node*> child_elements(DOM::Node* node) = 0;

    // --- Node metadata for wrappers (7.1.1) ---
    virtual NodeKind node_kind(const DOM::Node* node) = 0;
    // Uppercase tag name for elements ("DIV"), "#text" for text nodes, "" otherwise.
    virtual std::string node_name(const DOM::Node* node) = 0;
};

}  // namespace Hummingbird
