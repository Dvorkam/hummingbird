#pragma once

#include <algorithm>
#include <type_traits>
#include <utility>
#include <vector>

#include "core/platform_api/IGraphicsContext.h"
#include "layout/Geometry.h"
#include "layout/RenderObject.h"
#include "layout/RenderTreeTraversal.h"
#include "style/ComputedStyle.h"

namespace Hummingbird::Layout::Positioning {

inline bool is_positioned(const Css::ComputedStyle* style) {
    return style && style->position != Css::ComputedStyle::Position::Static;
}

inline bool is_absolute(const Css::ComputedStyle* style) {
    return style && style->position == Css::ComputedStyle::Position::Absolute;
}

inline int z_index_for(const Css::ComputedStyle* style) {
    if (!style || style->position == Css::ComputedStyle::Position::Static) {
        return 0;
    }
    return style->z_index.value_or(0);
}

inline void collect_children_in_paint_order(const RenderObject& node, std::vector<RenderObject*>& out,
                                            Traversal::ChildOrder order) {
    out.clear();
    out.reserve(node.get_children().size());
    for (const auto& child : node.get_children()) {
        out.push_back(child.get());
    }
    std::stable_sort(out.begin(), out.end(), [](const RenderObject* a, const RenderObject* b) {
        const auto* style_a = a ? a->get_computed_style() : nullptr;
        const auto* style_b = b ? b->get_computed_style() : nullptr;
        return z_index_for(style_a) < z_index_for(style_b);
    });
    if (order == Traversal::ChildOrder::Reverse) {
        std::reverse(out.begin(), out.end());
    }
}

template <Traversal::RenderNode NodeT, typename Enter, typename Exit>
bool traverse_render_tree_z_order(NodeT& node, const Point& offset, Enter&& enter, Exit&& exit,
                                  Traversal::ChildOrder child_order = Traversal::ChildOrder::Forward) {
    const auto& rect = node.get_rect();
    Rect absolute{offset.x + rect.x, offset.y + rect.y, rect.width, rect.height};

    Traversal::TraverseAction action = enter(node, absolute, offset);
    if (action == Traversal::TraverseAction::Stop) return false;
    if (action == Traversal::TraverseAction::SkipChildren) return true;

    std::vector<RenderObject*> ordered_children;
    collect_children_in_paint_order(node, ordered_children, child_order);
    using ChildT = std::conditional_t<std::is_const_v<NodeT>, const RenderObject, RenderObject>;
    for (auto* child_ptr : ordered_children) {
        auto* child_node = static_cast<ChildT*>(child_ptr);
        Point child_offset{absolute.x, absolute.y};
        if (!traverse_render_tree_z_order(*child_node, child_offset, enter, exit, child_order)) return false;
    }

    action = exit(node, absolute, offset);
    return action != Traversal::TraverseAction::Stop;
}

template <Traversal::RenderNode NodeT, typename Enter>
bool traverse_render_tree_z_order(NodeT& node, const Point& offset, Enter&& enter,
                                  Traversal::ChildOrder child_order = Traversal::ChildOrder::Forward) {
    auto noop_exit = [](auto&, const Rect&, const Point&) { return Traversal::TraverseAction::Continue; };
    return traverse_render_tree_z_order(node, offset, std::forward<Enter>(enter), noop_exit, child_order);
}

void apply_positioning(RenderObject& root, IGraphicsContext& context, const Rect& viewport);

}  // namespace Hummingbird::Layout::Positioning
