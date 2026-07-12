#pragma once

#include <algorithm>
#include <type_traits>
#include <utility>
#include <vector>

#include "core/platform_api/IGraphicsContext.h"
#include "layout/RenderObject.h"
#include "layout/geometry/Geometry.h"
#include "layout/paint/RenderTreeTraversal.h"
#include "style/types/ComputedStyle.h"

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
    using ChildT = std::conditional_t<std::is_const_v<NodeT>, const RenderObject, RenderObject>;
    auto resolve_offset = [](const auto& current, const Point& current_offset) {
        const auto* style = current.get_computed_style();
        Point transform_offset{};
        if (style && style->transform_has_translate) {
            transform_offset.x = style->transform_translate_x;
            transform_offset.y = style->transform_translate_y;
        }
        return Point{current_offset.x + transform_offset.x, current_offset.y + transform_offset.y};
    };
    auto for_each_child = [child_order](NodeT& current, auto&& visitor) {
        std::vector<RenderObject*> ordered_children;
        collect_children_in_paint_order(current, ordered_children, child_order);
        for (auto* child_ptr : ordered_children) {
            auto* child_node = static_cast<ChildT*>(child_ptr);
            if (!visitor(child_node)) {
                return false;
            }
        }
        return true;
    };
    return Traversal::traverse_render_tree_custom(node, offset, std::forward<Enter>(enter), std::forward<Exit>(exit),
                                                  for_each_child, resolve_offset);
}

template <Traversal::RenderNode NodeT, typename Enter>
bool traverse_render_tree_z_order(NodeT& node, const Point& offset, Enter&& enter,
                                  Traversal::ChildOrder child_order = Traversal::ChildOrder::Forward) {
    auto noop_exit = [](auto&, const Rect&, const Point&) { return Traversal::TraverseAction::Continue; };
    return traverse_render_tree_z_order(node, offset, std::forward<Enter>(enter), noop_exit, child_order);
}

void apply_positioning(RenderObject& root, IGraphicsContext& context, const Rect& viewport);

}  // namespace Hummingbird::Layout::Positioning
