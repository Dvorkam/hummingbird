#pragma once

#include <concepts>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

#include "core/ArenaAllocator.h"

namespace Hummingbird::Css {
struct ComputedStyle;
}

namespace Hummingbird::DOM {

class Node;

template <typename T>
concept NodeLike = std::derived_from<T, Node>;

class Node {
public:
    virtual ~Node() = default;

    template <NodeLike ChildT>
    void append_child(Core::ArenaPtr<ChildT> child) {
        child->m_parent = this;
        Node* raw = child.release();
        m_children.emplace_back(Core::ArenaPtr<Node>(raw));
    }

    const std::vector<Core::ArenaPtr<Node>>& get_children() const { return m_children; }
    Node* get_parent() { return m_parent; }
    const Node* get_parent() const { return m_parent; }
    void clear_children() { m_children.clear(); }

    // --- Post-parse tree surgery (Milestone 7 mutation primitives) ---
    // These move ownership of arena-backed child nodes without freeing arena
    // memory: a detached node's storage stays valid until the arena resets, so
    // it can be re-inserted. Ownership always lives in exactly one place — a
    // parent's m_children or a caller-held ArenaPtr (see DocumentScriptHost's
    // detached set). See doc/dev_guide/dom_arena_ownership.md.

    // Appends child at the end of this node's child list, taking ownership.
    void append_child_node(Core::ArenaPtr<Node> child) {
        child->m_parent = this;
        m_children.emplace_back(std::move(child));
    }

    // Inserts child immediately before `ref` (a current child of this node);
    // appends when ref is null. Returns false if ref is non-null but not a child.
    bool insert_child_before(Core::ArenaPtr<Node> child, const Node* ref) {
        if (!ref) {
            append_child_node(std::move(child));
            return true;
        }
        for (auto it = m_children.begin(); it != m_children.end(); ++it) {
            if (it->get() == ref) {
                child->m_parent = this;
                m_children.insert(it, std::move(child));
                return true;
            }
        }
        return false;
    }

    // Detaches `child` from this node, returning the owning pointer (empty if
    // child is not a direct child). The returned node keeps its arena storage;
    // its parent link is cleared.
    Core::ArenaPtr<Node> remove_child_node(Node* child) {
        for (auto it = m_children.begin(); it != m_children.end(); ++it) {
            if (it->get() == child) {
                Core::ArenaPtr<Node> owned = std::move(*it);
                m_children.erase(it);
                owned->m_parent = nullptr;
                return owned;
            }
        }
        return {};
    }

    // True if this node is `other` or one of its ancestors — used to reject
    // hierarchy-violating inserts (a node cannot contain itself).
    bool is_inclusive_ancestor_of(const Node* other) const {
        for (const Node* cursor = other; cursor; cursor = cursor->m_parent) {
            if (cursor == this) return true;
        }
        return false;
    }

    Node* first_child() { return m_children.empty() ? nullptr : m_children.front().get(); }
    Node* last_child() { return m_children.empty() ? nullptr : m_children.back().get(); }

    Node* next_sibling() { return sibling_at_offset(+1); }
    Node* previous_sibling() { return sibling_at_offset(-1); }

    void set_computed_style(std::shared_ptr<Css::ComputedStyle> style) { m_computed_style = std::move(style); }
    std::shared_ptr<const Css::ComputedStyle> get_computed_style() const { return m_computed_style; }

protected:
    Node() = default;

    // Returns the sibling `offset` positions away (only ±1 is used) within the
    // parent's child list, or null at the ends / when detached.
    Node* sibling_at_offset(int offset) {
        if (!m_parent) return nullptr;
        const auto& siblings = m_parent->m_children;
        for (size_t i = 0; i < siblings.size(); ++i) {
            if (siblings[i].get() == this) {
                const long target = static_cast<long>(i) + offset;
                if (target < 0 || target >= static_cast<long>(siblings.size())) return nullptr;
                return siblings[static_cast<size_t>(target)].get();
            }
        }
        return nullptr;
    }

    Node* m_parent = nullptr;
    std::vector<Core::ArenaPtr<Node>> m_children;
    std::shared_ptr<Css::ComputedStyle> m_computed_style;
};

}  // namespace Hummingbird::DOM
