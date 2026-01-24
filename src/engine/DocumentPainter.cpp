#include "engine/DocumentPainter.h"

#include "layout/RenderObject.h"

namespace Hummingbird::Engine {

void DocumentPainter::paint(const Layout::RenderObject* render_tree, IGraphicsContext& graphics,
                            const Layout::Rect& viewport, bool debug_outlines, float scroll_y,
                            const DocumentInputController& input_controller) {
    if (!render_tree) return;

    graphics.set_viewport(viewport);

    Renderer::PaintOptions opts;
    opts.debug_outlines = debug_outlines;
    opts.scroll_y = scroll_y;
    opts.viewport = viewport;

    painter_.paint(*render_tree, graphics, opts);
    input_controller.paint_controls(render_tree, graphics, viewport, scroll_y);
}

}  // namespace Hummingbird::Engine
