#pragma once

#include <concepts>
#include <type_traits>
#include <utility>

#include "layout/geometry/Geometry.h"
#include "layout/RenderObject.h"

namespace Hummingbird::Layout::Traversal {

template <typename T>
concept RenderNode = std::is_base_of_v<RenderObject, std::remove_cvref_t<T>>;

enum class ChildOrder {
    Forward,
    Reverse,
};

enum class TraverseAction {
    Continue,
    SkipChildren,
    Stop,
};

namespace Detail {
template <RenderNode NodeT, typename Enter, typename Exit>
bool traverse_render_tree(NodeT* node, const Point& offset, Enter&& enter, Exit&& exit, ChildOrder child_order) {
    if (!node) return true;
    const auto& rect = node->get_rect();
    Rect absolute{offset.x + rect.x, offset.y + rect.y, rect.width, rect.height};

    TraverseAction action = enter(*node, absolute, offset);
    if (action == TraverseAction::Stop) return false;
    if (action == TraverseAction::SkipChildren) return true;

    const auto& children = node->get_children();
    using ChildT = std::conditional_t<std::is_const_v<NodeT>, const RenderObject, RenderObject>;

    if (child_order == ChildOrder::Forward) {
        for (const auto& child : children) {
            auto* child_ptr = child.get();
            auto* child_node = static_cast<ChildT*>(child_ptr);
            Point child_offset{absolute.x, absolute.y};
            if (!traverse_render_tree(child_node, child_offset, enter, exit, child_order)) return false;
        }
    } else {
        for (auto it = children.rbegin(); it != children.rend(); ++it) {
            auto* child_ptr = it->get();
            auto* child_node = static_cast<ChildT*>(child_ptr);
            Point child_offset{absolute.x, absolute.y};
            if (!traverse_render_tree(child_node, child_offset, enter, exit, child_order)) return false;
        }
    }

    action = exit(*node, absolute, offset);
    return action != TraverseAction::Stop;
}
}  // namespace Detail

template <RenderNode NodeT, typename Enter, typename Exit>
bool traverse_render_tree(NodeT& node, const Point& offset, Enter&& enter, Exit&& exit,
                          ChildOrder child_order = ChildOrder::Forward) {
    return Detail::traverse_render_tree(&node, offset, std::forward<Enter>(enter), std::forward<Exit>(exit),
                                        child_order);
}

template <RenderNode NodeT, typename Enter>
bool traverse_render_tree(NodeT& node, const Point& offset, Enter&& enter,
                          ChildOrder child_order = ChildOrder::Forward) {
    auto noop_exit = [](auto&, const Rect&, const Point&) { return TraverseAction::Continue; };
    return traverse_render_tree(node, offset, std::forward<Enter>(enter), noop_exit, child_order);
}

}  // namespace Hummingbird::Layout::Traversal
