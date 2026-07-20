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

    // --- Attributes / classList / dataset (7.1.2) ---
    virtual bool has_attribute(DOM::Node* node, std::string_view name) = 0;
    // Attribute value, or "" when absent (pair with has_attribute for null checks).
    virtual std::string get_attribute(DOM::Node* node, std::string_view name) = 0;
    virtual void remove_attribute(DOM::Node* node, std::string_view name) = 0;

    virtual bool class_list_contains(DOM::Node* node, std::string_view token) = 0;
    virtual void class_list_add(DOM::Node* node, std::string_view token) = 0;
    virtual void class_list_remove(DOM::Node* node, std::string_view token) = 0;
    // Adds the token when absent / removes it when present; returns final membership.
    virtual bool class_list_toggle(DOM::Node* node, std::string_view token) = 0;

    // dataset uses the DOM camelCase<->data-* mapping ("userId" <-> "data-user-id").
    // Returns true (and fills out) when the mapped attribute is present.
    virtual bool get_dataset(DOM::Node* node, std::string_view key, std::string& out) = 0;
    virtual void set_dataset(DOM::Node* node, std::string_view key, std::string_view value) = 0;

    // --- Form control surface (7.1.5) ---
    // Reflect the form-control state JS reads/writes. `value` is the control's
    // current value; `checked`/`disabled` are boolean attribute state. Focus is
    // reflected as the :focus pseudo-state (full text-edit focus wiring + the
    // checkbox control land with the event system in 7.2.4).
    virtual std::string get_value(DOM::Node* node) = 0;
    virtual void set_value(DOM::Node* node, std::string_view value) = 0;
    virtual bool get_checked(DOM::Node* node) = 0;
    virtual void set_checked(DOM::Node* node, bool checked) = 0;
    virtual bool get_disabled(DOM::Node* node) = 0;
    virtual void set_disabled(DOM::Node* node, bool disabled) = 0;
    virtual void set_focused(DOM::Node* node, bool focused) = 0;

    // --- innerHTML (7.1.4) ---
    // Replaces node's children with the fragment parsed from `html` (reuses the
    // document HTML parser in a recovery-oriented fragment mode).
    virtual void set_inner_html(DOM::Node* node, std::string_view html) = 0;
    // Serializes node's children back to HTML.
    virtual std::string get_inner_html(DOM::Node* node) = 0;

    // --- Selector queries (7.1.3) ---
    // `scope` is the element to search within; nullptr means the document root.
    // Queries reuse the style engine's selector parser + matcher, so the
    // supported subset is identical to CSS. Results are document-order snapshots.
    virtual DOM::Node* query_selector(DOM::Node* scope, std::string_view selector) = 0;
    virtual std::vector<DOM::Node*> query_selector_all(DOM::Node* scope, std::string_view selector) = 0;
    virtual bool matches(DOM::Node* node, std::string_view selector) = 0;
    virtual DOM::Node* closest(DOM::Node* node, std::string_view selector) = 0;
    // Legacy live-collection accessors (returned here as static snapshots).
    virtual std::vector<DOM::Node*> get_elements_by_class_name(DOM::Node* scope, std::string_view names) = 0;
    virtual std::vector<DOM::Node*> get_elements_by_tag_name(DOM::Node* scope, std::string_view tag) = 0;

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
