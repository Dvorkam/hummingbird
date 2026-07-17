#include "engine/script/DocumentScriptHost.h"

#include <utility>

#include "core/ArenaAllocator.h"
#include "core/dom/Element.h"
#include "core/dom/Node.h"
#include "core/dom/Text.h"
#include "core/utils/StringUtils.h"
#include "html/HtmlAttributeNames.h"

namespace Hummingbird::Engine {

namespace {
DOM::Element* find_element_by_id(DOM::Node* node, std::string_view id) {
    if (!node) return nullptr;

    if (auto* element = dynamic_cast<DOM::Element*>(node)) {
        if (const auto* attr = element->find_attribute(Hummingbird::Html::AttributeNames::Id)) {
            if (*attr == id) {
                return element;
            }
        }
    }

    for (const auto& child : node->get_children()) {
        if (auto* match = find_element_by_id(child.get(), id)) {
            return match;
        }
    }
    return nullptr;
}

void collect_text(const DOM::Node* node, std::string& out) {
    if (!node) return;
    if (auto* text_node = dynamic_cast<const DOM::Text*>(node)) {
        out.append(text_node->get_text());
        return;
    }
    for (const auto& child : node->get_children()) {
        collect_text(child.get(), out);
    }
}

// Walks siblings from `node` in the given direction until an element is found.
DOM::Node* element_sibling(DOM::Node* node, bool forward) {
    if (!node) return nullptr;
    DOM::Node* cursor = forward ? node->next_sibling() : node->previous_sibling();
    while (cursor) {
        if (dynamic_cast<DOM::Element*>(cursor)) {
            return cursor;
        }
        cursor = forward ? cursor->next_sibling() : cursor->previous_sibling();
    }
    return nullptr;
}
}  // namespace

DocumentScriptHost::DocumentScriptHost() = default;
DocumentScriptHost::~DocumentScriptHost() = default;

void DocumentScriptHost::reset(DOM::Node* root, Core::ArenaAllocator* arena) {
    root_ = root;
    arena_ = arena;
    mutated_ = false;
    // Detached nodes from a previous document point into an arena that is about
    // to be (or has been) reset; drop them so no stale pointer survives.
    detached_.clear();
}

void DocumentScriptHost::clear() {
    root_ = nullptr;
    arena_ = nullptr;
    mutated_ = false;
    detached_.clear();
}

bool DocumentScriptHost::consume_mutations() {
    bool result = mutated_;
    mutated_ = false;
    return result;
}

DOM::Element* DocumentScriptHost::get_element_by_id(std::string_view id) {
    if (!root_ || id.empty()) {
        return nullptr;
    }
    return find_element_by_id(root_, id);
}

std::string DocumentScriptHost::get_text_content(const DOM::Node* node) {
    if (!node) return {};
    std::string text;
    collect_text(node, text);
    return text;
}

void DocumentScriptHost::set_text_content(DOM::Node* node, std::string_view text) {
    if (!node) return;
    if (auto* text_node = dynamic_cast<DOM::Text*>(node)) {
        text_node->set_text(text);
        mutated_ = true;
        return;
    }
    if (!arena_) return;
    node->clear_children();
    if (!text.empty()) {
        node->append_child(DOM::Text::create(*arena_, text));
    }
    mutated_ = true;
}

void DocumentScriptHost::set_attribute(DOM::Node* node, std::string_view name, std::string_view value) {
    auto* element = dynamic_cast<DOM::Element*>(node);
    if (!element) return;
    element->set_attribute(name, value);
    mutated_ = true;
}

DOM::Element* DocumentScriptHost::create_element(std::string_view tag_name) {
    if (!arena_) return nullptr;
    auto element = DOM::Element::create(*arena_, tag_name);
    if (!element) return nullptr;
    DOM::Element* raw = element.get();
    detached_.emplace_back(Core::ArenaPtr<DOM::Node>(element.release()));
    return raw;
}

DOM::Node* DocumentScriptHost::create_text_node(std::string_view data) {
    if (!arena_) return nullptr;
    auto text = DOM::Text::create(*arena_, data);
    if (!text) return nullptr;
    DOM::Node* raw = text.get();
    detached_.emplace_back(Core::ArenaPtr<DOM::Node>(text.release()));
    return raw;
}

Core::ArenaPtr<DOM::Node> DocumentScriptHost::take_ownership(DOM::Node* node) {
    if (!node) return {};
    if (DOM::Node* parent = node->get_parent()) {
        return parent->remove_child_node(node);
    }
    for (auto it = detached_.begin(); it != detached_.end(); ++it) {
        if (it->get() == node) {
            Core::ArenaPtr<DOM::Node> owned = std::move(*it);
            detached_.erase(it);
            return owned;
        }
    }
    return {};
}

DOM::Node* DocumentScriptHost::append_child(DOM::Node* parent, DOM::Node* child) {
    return insert_before(parent, child, nullptr);
}

DOM::Node* DocumentScriptHost::insert_before(DOM::Node* parent, DOM::Node* child, DOM::Node* reference) {
    // Only elements can host children (text/other nodes are leaves).
    if (!dynamic_cast<DOM::Element*>(parent) || !child) {
        return nullptr;
    }
    // A node cannot become its own descendant, and the reference must belong to
    // the parent when provided.
    if (child->is_inclusive_ancestor_of(parent)) {
        return nullptr;
    }
    if (reference && reference->get_parent() != parent) {
        return nullptr;
    }
    Core::ArenaPtr<DOM::Node> owned = take_ownership(child);
    if (!owned) {
        return nullptr;
    }
    if (!parent->insert_child_before(std::move(owned), reference)) {
        return nullptr;
    }
    mutated_ = true;
    return child;
}

DOM::Node* DocumentScriptHost::remove_child(DOM::Node* parent, DOM::Node* child) {
    if (!parent || !child || child->get_parent() != parent) {
        return nullptr;
    }
    Core::ArenaPtr<DOM::Node> owned = parent->remove_child_node(child);
    if (!owned) {
        return nullptr;
    }
    detached_.emplace_back(std::move(owned));
    mutated_ = true;
    return child;
}

DOM::Node* DocumentScriptHost::replace_child(DOM::Node* parent, DOM::Node* new_child, DOM::Node* old_child) {
    if (!dynamic_cast<DOM::Element*>(parent) || !new_child || !old_child) {
        return nullptr;
    }
    if (old_child->get_parent() != parent) {
        return nullptr;
    }
    if (new_child->is_inclusive_ancestor_of(parent)) {
        return nullptr;
    }
    // Insert the replacement immediately before the outgoing node, then detach
    // the outgoing node. new_child == old_child is a no-op that keeps old_child.
    if (new_child != old_child) {
        Core::ArenaPtr<DOM::Node> incoming = take_ownership(new_child);
        if (!incoming) {
            return nullptr;
        }
        if (!parent->insert_child_before(std::move(incoming), old_child)) {
            return nullptr;
        }
        Core::ArenaPtr<DOM::Node> outgoing = parent->remove_child_node(old_child);
        if (outgoing) {
            detached_.emplace_back(std::move(outgoing));
        }
    }
    mutated_ = true;
    return old_child;
}

DOM::Node* DocumentScriptHost::parent_node(DOM::Node* node) {
    return node ? node->get_parent() : nullptr;
}
DOM::Node* DocumentScriptHost::first_child(DOM::Node* node) {
    return node ? node->first_child() : nullptr;
}
DOM::Node* DocumentScriptHost::last_child(DOM::Node* node) {
    return node ? node->last_child() : nullptr;
}
DOM::Node* DocumentScriptHost::next_sibling(DOM::Node* node) {
    return node ? node->next_sibling() : nullptr;
}
DOM::Node* DocumentScriptHost::previous_sibling(DOM::Node* node) {
    return node ? node->previous_sibling() : nullptr;
}

DOM::Node* DocumentScriptHost::next_element_sibling(DOM::Node* node) {
    return element_sibling(node, /*forward=*/true);
}
DOM::Node* DocumentScriptHost::previous_element_sibling(DOM::Node* node) {
    return element_sibling(node, /*forward=*/false);
}

std::vector<DOM::Node*> DocumentScriptHost::child_nodes(DOM::Node* node) {
    std::vector<DOM::Node*> result;
    if (!node) return result;
    const auto& children = node->get_children();
    result.reserve(children.size());
    for (const auto& child : children) {
        result.push_back(child.get());
    }
    return result;
}

std::vector<DOM::Node*> DocumentScriptHost::child_elements(DOM::Node* node) {
    std::vector<DOM::Node*> result;
    if (!node) return result;
    for (const auto& child : node->get_children()) {
        if (dynamic_cast<DOM::Element*>(child.get())) {
            result.push_back(child.get());
        }
    }
    return result;
}

NodeKind DocumentScriptHost::node_kind(const DOM::Node* node) {
    if (dynamic_cast<const DOM::Element*>(node)) return NodeKind::Element;
    if (dynamic_cast<const DOM::Text*>(node)) return NodeKind::Text;
    return NodeKind::Other;
}

std::string DocumentScriptHost::node_name(const DOM::Node* node) {
    if (auto* element = dynamic_cast<const DOM::Element*>(node)) {
        return Core::Utils::to_upper(element->get_tag_name());
    }
    if (dynamic_cast<const DOM::Text*>(node)) {
        return "#text";
    }
    return {};
}

}  // namespace Hummingbird::Engine
