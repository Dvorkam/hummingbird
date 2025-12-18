#pragma once

#include <vector>
#include <memory>

namespace Hummingbird::DOM {

    class Node {
    public:
        virtual ~Node() = default;

        void append_child(std::unique_ptr<Node> child) {
            child->m_parent = this;
            m_children.push_back(std::move(child));
        }

        const std::vector<std::unique_ptr<Node>>& get_children() const { return m_children; }
        Node* get_parent() const { return m_parent; }

    protected:
        Node* m_parent = nullptr;
        std::vector<std::unique_ptr<Node>> m_children;
    };

}
