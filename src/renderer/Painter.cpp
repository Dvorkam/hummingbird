#include "renderer/Painter.h"

#include "core/platform_api/IGraphicsContext.h"
#include "layout/RenderObject.h"

namespace Hummingbird::Renderer {

namespace {

void draw_outline(IGraphicsContext& context, const Layout::Rect& rect, const Color& color) {
    constexpr float kThickness = 1.0f;
    Layout::Rect top{rect.x, rect.y, rect.width, kThickness};
    Layout::Rect bottom{rect.x, rect.y + rect.height - kThickness, rect.width, kThickness};
    Layout::Rect left{rect.x, rect.y, kThickness, rect.height};
    Layout::Rect right{rect.x + rect.width - kThickness, rect.y, kThickness, rect.height};
    context.fill_rect(top, color);
    context.fill_rect(bottom, color);
    context.fill_rect(left, color);
    context.fill_rect(right, color);
}

bool intersects(const Layout::Rect& a, const Layout::Rect& b) {
    if (a.width <= 0.0f || a.height <= 0.0f) return false;
    if (b.width <= 0.0f || b.height <= 0.0f) return false;
    return !(a.x + a.width <= b.x || a.x >= b.x + b.width || a.y + a.height <= b.y || a.y >= b.y + b.height);
}

template <typename Visitor>
void traverse_tree(Layout::RenderObject& node, const Layout::Point& offset, Visitor&& visitor) {
    const auto& rect = node.get_rect();
    Layout::Rect absolute{offset.x + rect.x, offset.y + rect.y, rect.width, rect.height};
    if (!visitor(node, absolute, offset)) {
        return;
    }

    for (const auto& child : node.get_children()) {
        Layout::Point child_offset{absolute.x, absolute.y};
        traverse_tree(*child, child_offset, visitor);
    }
}

void paint_tree_culled(Layout::RenderObject& node, IGraphicsContext& context, const Layout::Point& offset,
                       const Layout::Rect& viewport) {
    traverse_tree(node, offset, [&](Layout::RenderObject& current, const Layout::Rect& absolute,
                                    const Layout::Point& local_offset) {
        if (!intersects(absolute, viewport)) {
            return false;
        }
        current.paint_self(context, local_offset);
        return true;
    });
}

void paint_debug_outlines(Layout::RenderObject& node, IGraphicsContext& context, const Layout::Point& offset,
                          const Color& color) {
    traverse_tree(node, offset, [&](Layout::RenderObject& /*current*/, const Layout::Rect& absolute,
                                    const Layout::Point& /*local_offset*/) {
        draw_outline(context, absolute, color);
        return true;
    });
}

}  // namespace

void Painter::paint(Layout::RenderObject& root, IGraphicsContext& context, const PaintOptions& options) {
    context.set_viewport(options.viewport);
    // Start the recursive paint process from the root with scroll offset applied.
    Layout::Point offset{0, -options.scroll_y};
    if (options.viewport.width <= 0.0f || options.viewport.height <= 0.0f) {
        root.paint(context, offset);
    } else {
        paint_tree_culled(root, context, offset, options.viewport);
    }
    if (options.debug_outlines) {
        Color outline{255, 0, 0, 100};
        paint_debug_outlines(root, context, offset, outline);
    }
}

}  // namespace Hummingbird::Renderer
