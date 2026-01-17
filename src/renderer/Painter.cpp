#include "renderer/Painter.h"

#include "core/platform_api/IGraphicsContext.h"
#include "layout/GeometryUtils.h"
#include "layout/RenderObject.h"
#include "layout/RenderTreeTraversal.h"

namespace Hummingbird::Renderer {

namespace {
constexpr Color kOutlineColor{255, 0, 0, 100};

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

struct PaintContext {
    Layout::Point offset;
    const Layout::Rect* viewport = nullptr;
    bool debug_outlines = false;
};

void paint_tree(const Layout::RenderObject& node, IGraphicsContext& context, const PaintContext& paint_context) {
    Layout::Traversal::traverse_render_tree(
        node, paint_context.offset,
        [&](const Layout::RenderObject& current, const Layout::Rect& absolute, const Layout::Point& local_offset) {
            if (paint_context.viewport && !Layout::rect_intersects(absolute, *paint_context.viewport)) {
                return Layout::Traversal::TraverseAction::SkipChildren;
            }
            current.paint_self(context, local_offset);
            if (paint_context.debug_outlines) {
                draw_outline(context, absolute, kOutlineColor);
            }
            return Layout::Traversal::TraverseAction::Continue;
        });
}

}  // namespace

void Painter::paint(const Layout::RenderObject& root, IGraphicsContext& context, const PaintOptions& options) {
    context.set_viewport(options.viewport);
    // Start the recursive paint process from the root with scroll offset applied.
    PaintContext paint_context;
    paint_context.offset = {0, -options.scroll_y};
    paint_context.viewport =
        (options.viewport.width > 0.0f && options.viewport.height > 0.0f) ? &options.viewport : nullptr;
    paint_context.debug_outlines = options.debug_outlines;
    paint_tree(root, context, paint_context);
}

}  // namespace Hummingbird::Renderer
