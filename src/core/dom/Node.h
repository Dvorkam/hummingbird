#pragma once

#include <concepts>
#include <memory>
#include <type_traits>
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

    void set_computed_style(std::shared_ptr<Css::ComputedStyle> style) { m_computed_style = std::move(style); }
    std::shared_ptr<const Css::ComputedStyle> get_computed_style() const { return m_computed_style; }

protected:
    Node() = default;

    Node* m_parent = nullptr;
    std::vector<Core::ArenaPtr<Node>> m_children;
    std::shared_ptr<Css::ComputedStyle> m_computed_style;
};

}  // namespace Hummingbird::DOM
