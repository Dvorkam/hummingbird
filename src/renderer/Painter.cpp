#include "renderer/Painter.h"

#include "core/IGraphicsContext.h"
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

}  // namespace

void Painter::paint(Layout::RenderObject& root, IGraphicsContext& context, const PaintOptions& options) {
    context.set_viewport(options.viewport);
    // Start the recursive paint process from the root with scroll offset applied.
    Layout::Point offset{0, -options.scroll_y};
    root.paint(context, offset);
    if (options.debug_outlines) {
        Color outline{255, 0, 0, 100};
        debug_outline_tree(root, context, offset, outline);
    }
}

}  // namespace Hummingbird::Renderer
