#pragma once

#include <concepts>
#include <type_traits>
#include <utility>

#include "layout/RenderObject.h"
#include "layout/geometry/Geometry.h"

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
template <RenderNode NodeT, typename Enter, typename Exit, typename ChildForEach, typename OffsetResolver>
bool traverse_render_tree(NodeT* node, const Point& offset, Enter&& enter, Exit&& exit, ChildForEach&& for_each_child,
                          OffsetResolver&& resolve_offset) {
    if (!node) {
        return true;
    }
    const auto& rect = node->get_rect();
    Point effective_offset = resolve_offset(*node, offset);
    Rect absolute{effective_offset.x + rect.x, effective_offset.y + rect.y, rect.width, rect.height};

    TraverseAction action = enter(*node, absolute, effective_offset);
    if (action == TraverseAction::Stop) {
        return false;
    }
    if (action == TraverseAction::SkipChildren) {
        return true;
    }

    using ChildT = std::conditional_t<std::is_const_v<NodeT>, const RenderObject, RenderObject>;
    auto visit_child = [&](ChildT* child_node) {
        Point child_offset{absolute.x, absolute.y};
        return traverse_render_tree(child_node, child_offset, enter, exit, for_each_child, resolve_offset);
    };

    if (!for_each_child(*node, visit_child)) {
        return false;
    }

    action = exit(*node, absolute, effective_offset);
    return action != TraverseAction::Stop;
}
}  // namespace Detail

template <RenderNode NodeT, typename Enter, typename Exit>
bool traverse_render_tree(NodeT& node, const Point& offset, Enter&& enter, Exit&& exit,
                          ChildOrder child_order = ChildOrder::Forward) {
    using ChildT = std::conditional_t<std::is_const_v<NodeT>, const RenderObject, RenderObject>;
    auto for_each_child = [child_order](NodeT& current, auto&& visitor) {
        const auto& children = current.get_children();
        if (child_order == ChildOrder::Forward) {
            for (const auto& child : children) {
                auto* child_node = static_cast<ChildT*>(child.get());
                if (!visitor(child_node)) {
                    return false;
                }
            }
        } else {
            for (auto it = children.rbegin(); it != children.rend(); ++it) {
                auto* child_node = static_cast<ChildT*>(it->get());
                if (!visitor(child_node)) {
                    return false;
                }
            }
        }
        return true;
    };
    auto resolve_offset = [](const auto&, const Point& current_offset) { return current_offset; };
    return Detail::traverse_render_tree(&node, offset, std::forward<Enter>(enter), std::forward<Exit>(exit),
                                        for_each_child, resolve_offset);
}

template <RenderNode NodeT, typename Enter>
bool traverse_render_tree(NodeT& node, const Point& offset, Enter&& enter,
                          ChildOrder child_order = ChildOrder::Forward) {
    auto noop_exit = [](auto&, const Rect&, const Point&) { return TraverseAction::Continue; };
    return traverse_render_tree(node, offset, std::forward<Enter>(enter), noop_exit, child_order);
}

template <RenderNode NodeT, typename Enter, typename Exit, typename ChildForEach, typename OffsetResolver>
bool traverse_render_tree_custom(NodeT& node, const Point& offset, Enter&& enter, Exit&& exit,
                                 ChildForEach&& for_each_child, OffsetResolver&& resolve_offset) {
    return Detail::traverse_render_tree(&node, offset, std::forward<Enter>(enter), std::forward<Exit>(exit),
                                        std::forward<ChildForEach>(for_each_child),
                                        std::forward<OffsetResolver>(resolve_offset));
}

}  // namespace Hummingbird::Layout::Traversal
