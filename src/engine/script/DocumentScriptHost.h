#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "core/ArenaAllocator.h"
#include "core/platform_api/IScriptHost.h"

namespace Hummingbird::DOM {
class Element;
class Node;
}  // namespace Hummingbird::DOM

namespace Hummingbird::Engine {

class DocumentScriptHost final : public IScriptHost {
public:
    DocumentScriptHost();
    ~DocumentScriptHost() override;

    void reset(DOM::Node* root, Core::ArenaAllocator* arena);
    void clear();
    bool consume_mutations();

    DOM::Element* get_element_by_id(std::string_view id) override;
    std::string get_text_content(const DOM::Node* node) override;
    void set_text_content(DOM::Node* node, std::string_view text) override;
    void set_attribute(DOM::Node* node, std::string_view name, std::string_view value) override;

    bool has_attribute(DOM::Node* node, std::string_view name) override;
    std::string get_attribute(DOM::Node* node, std::string_view name) override;
    void remove_attribute(DOM::Node* node, std::string_view name) override;

    bool class_list_contains(DOM::Node* node, std::string_view token) override;
    void class_list_add(DOM::Node* node, std::string_view token) override;
    void class_list_remove(DOM::Node* node, std::string_view token) override;
    bool class_list_toggle(DOM::Node* node, std::string_view token) override;

    bool get_dataset(DOM::Node* node, std::string_view key, std::string& out) override;
    void set_dataset(DOM::Node* node, std::string_view key, std::string_view value) override;

    DOM::Node* query_selector(DOM::Node* scope, std::string_view selector) override;
    std::vector<DOM::Node*> query_selector_all(DOM::Node* scope, std::string_view selector) override;
    bool matches(DOM::Node* node, std::string_view selector) override;
    DOM::Node* closest(DOM::Node* node, std::string_view selector) override;
    std::vector<DOM::Node*> get_elements_by_class_name(DOM::Node* scope, std::string_view names) override;
    std::vector<DOM::Node*> get_elements_by_tag_name(DOM::Node* scope, std::string_view tag) override;

    DOM::Element* create_element(std::string_view tag_name) override;
    DOM::Node* create_text_node(std::string_view data) override;

    DOM::Node* append_child(DOM::Node* parent, DOM::Node* child) override;
    DOM::Node* insert_before(DOM::Node* parent, DOM::Node* child, DOM::Node* reference) override;
    DOM::Node* remove_child(DOM::Node* parent, DOM::Node* child) override;
    DOM::Node* replace_child(DOM::Node* parent, DOM::Node* new_child, DOM::Node* old_child) override;

    DOM::Node* parent_node(DOM::Node* node) override;
    DOM::Node* first_child(DOM::Node* node) override;
    DOM::Node* last_child(DOM::Node* node) override;
    DOM::Node* next_sibling(DOM::Node* node) override;
    DOM::Node* previous_sibling(DOM::Node* node) override;
    DOM::Node* next_element_sibling(DOM::Node* node) override;
    DOM::Node* previous_element_sibling(DOM::Node* node) override;
    std::vector<DOM::Node*> child_nodes(DOM::Node* node) override;
    std::vector<DOM::Node*> child_elements(DOM::Node* node) override;

    NodeKind node_kind(const DOM::Node* node) override;
    std::string node_name(const DOM::Node* node) override;

private:
    // Takes exclusive ownership of `node` out of wherever it currently lives
    // (its parent's child list or the detached set). Returns an empty pointer
    // when the node cannot be located or has no owning arena.
    Core::ArenaPtr<DOM::Node> take_ownership(DOM::Node* node);

    DOM::Node* root_ = nullptr;
    Core::ArenaAllocator* arena_ = nullptr;
    bool mutated_ = false;

    // Nodes created by script but not yet attached, plus nodes removed from the
    // tree. Their arena storage stays valid until the arena resets, so they can
    // be re-inserted; this vector is the single owner while they are detached.
    std::vector<Core::ArenaPtr<DOM::Node>> detached_;
};

}  // namespace Hummingbird::Engine
