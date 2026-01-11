#include "renderer/Painter.h"

#include "core/platform_api/IGraphicsContext.h"
#include "layout/GeometryUtils.h"
#include "layout/RenderObject.h"
#include "layout/RenderTreeTraversal.h"

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

void paint_tree(const Layout::RenderObject& node, IGraphicsContext& context, const Layout::Point& offset,
                const Layout::Rect* viewport, bool debug_outlines) {
    const Color outline{255, 0, 0, 100};
    Layout::Traversal::traverse_render_tree(
        node, offset,
        [&](const Layout::RenderObject& current, const Layout::Rect& absolute, const Layout::Point& local_offset) {
            if (viewport && !Layout::rect_intersects(absolute, *viewport)) {
                return Layout::Traversal::TraverseAction::SkipChildren;
            }
            current.paint_self(context, local_offset);
            if (debug_outlines) {
                draw_outline(context, absolute, outline);
            }
            return Layout::Traversal::TraverseAction::Continue;
        });
}

}  // namespace

void Painter::paint(const Layout::RenderObject& root, IGraphicsContext& context, const PaintOptions& options) {
    context.set_viewport(options.viewport);
    // Start the recursive paint process from the root with scroll offset applied.
    Layout::Point offset{0, -options.scroll_y};
    const Layout::Rect* viewport =
        (options.viewport.width > 0.0f && options.viewport.height > 0.0f) ? &options.viewport : nullptr;
    paint_tree(root, context, offset, viewport, options.debug_outlines);
}

}  // namespace Hummingbird::Renderer
