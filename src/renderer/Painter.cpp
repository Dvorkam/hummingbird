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

void debug_outline_tree(Layout::RenderObject& node, IGraphicsContext& context, const Layout::Point& offset,
                        const Color& color) {
    const auto& rect = node.get_rect();
    Layout::Rect absolute{offset.x + rect.x, offset.y + rect.y, rect.width, rect.height};
    draw_outline(context, absolute, color);

    for (const auto& child : node.get_children()) {
        Layout::Point child_offset{absolute.x, absolute.y};
        debug_outline_tree(*child, context, child_offset, color);
    }
}

bool intersects(const Layout::Rect& a, const Layout::Rect& b) {
    if (a.width <= 0.0f || a.height <= 0.0f) return false;
    if (b.width <= 0.0f || b.height <= 0.0f) return false;
    return !(a.x + a.width <= b.x || a.x >= b.x + b.width || a.y + a.height <= b.y || a.y >= b.y + b.height);
}

void paint_tree_culled(Layout::RenderObject& node, IGraphicsContext& context, const Layout::Point& offset,
                       const Layout::Rect& viewport) {
    const auto& rect = node.get_rect();
    Layout::Rect absolute{offset.x + rect.x, offset.y + rect.y, rect.width, rect.height};
    if (!intersects(absolute, viewport)) {
        return;
    }

    node.paint_self(context, offset);

    for (const auto& child : node.get_children()) {
        Layout::Point child_offset{absolute.x, absolute.y};
        paint_tree_culled(*child, context, child_offset, viewport);
    }
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
        debug_outline_tree(root, context, offset, outline);
    }
}

}  // namespace Hummingbird::Renderer
