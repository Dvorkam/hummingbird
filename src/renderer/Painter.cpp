#include "renderer/Painter.h"

#include <vector>

#include "core/platform_api/IGraphicsContext.h"
#include "layout/RenderObject.h"
#include "layout/geometry/GeometryUtils.h"
#include "layout/geometry/PositioningUtils.h"
#include "layout/paint/PaintUtils.h"

namespace Hummingbird::Renderer {

namespace {
constexpr Color kOutlineColor{255, 0, 0, 100};

struct PaintContext {
    Layout::Point offset;
    const Layout::Rect* viewport = nullptr;
    bool debug_outlines = false;
};

void paint_tree(const Layout::RenderObject& node, IGraphicsContext& context, const PaintContext& paint_context) {
    Layout::Positioning::traverse_render_tree_z_order(
        node, paint_context.offset,
        [&](const Layout::RenderObject& current, const Layout::Rect& absolute, const Layout::Point& local_offset) {
            if (paint_context.viewport && !Layout::rect_intersects(absolute, *paint_context.viewport) &&
                !current.has_absolute_descendant()) {
                return Layout::Traversal::TraverseAction::SkipChildren;
            }
            current.paint_self(context, local_offset);
            if (paint_context.debug_outlines) {
                Layout::PaintUtils::draw_outline(context, absolute, kOutlineColor);
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
