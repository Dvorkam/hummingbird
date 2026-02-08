#pragma once

#include <optional>
#include <utility>

#include "layout/geometry/GeometryUtils.h"
#include "layout/geometry/PositioningUtils.h"
#include "layout/paint/RenderTreeTraversal.h"

namespace Hummingbird::Layout {
class RenderObject;
}  // namespace Hummingbird::Layout

namespace Hummingbird::Engine::HitTest {

template <typename ResultT, typename ResolveFn>
std::optional<ResultT> hit_test_z_order(const Layout::RenderObject* render_tree, const Layout::Point& point,
                                        const Layout::Rect& viewport, float scroll_y, ResolveFn&& resolve) {
    if (!render_tree) {
        return std::nullopt;
    }
    if (!Layout::rect_contains_point(viewport, point)) {
        return std::nullopt;
    }

    Layout::Point offset{0.0f, -scroll_y};
    std::optional<ResultT> result;

    Layout::Positioning::traverse_render_tree_z_order(
        *render_tree, offset,
        [&](const Layout::RenderObject& node, const Layout::Rect& absolute, const Layout::Point& /*local_offset*/) {
            if (!Layout::rect_intersects(absolute, viewport) || !Layout::rect_contains_point(absolute, point)) {
                if (node.has_absolute_descendant()) {
                    return Layout::Traversal::TraverseAction::Continue;
                }
                return Layout::Traversal::TraverseAction::SkipChildren;
            }
            return Layout::Traversal::TraverseAction::Continue;
        },
        [&](const Layout::RenderObject& node, const Layout::Rect& /*absolute*/, const Layout::Point& /*local_offset*/) {
            auto hit = resolve(node);
            if (hit) {
                result = std::move(*hit);
                return Layout::Traversal::TraverseAction::Stop;
            }
            return Layout::Traversal::TraverseAction::Continue;
        },
        Layout::Traversal::ChildOrder::Reverse);

    return result;
}

}  // namespace Hummingbird::Engine::HitTest
