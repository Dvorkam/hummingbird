#include "engine/document/DocumentPainter.h"

#include "core/utils/Log.h"
#include "layout/RenderObject.h"

namespace Hummingbird::Engine {

namespace {
bool rect_equals(const Layout::Rect& a, const Layout::Rect& b) {
    return a.x == b.x && a.y == b.y && a.width == b.width && a.height == b.height;
}
}  // namespace

void DocumentPainter::paint(const Layout::RenderObject* render_tree, IGraphicsContext& graphics,
                            const Layout::Rect& viewport, bool debug_outlines, float scroll_y,
                            const DocumentInputController& input_controller) {
    if (!render_tree) return;

    graphics.set_viewport(viewport);

    const bool can_reuse = can_reuse_display_list(render_tree, viewport, debug_outlines, scroll_y);
    if (!can_reuse) {
        display_list_.clear();

        Renderer::PaintOptions opts;
        opts.debug_outlines = debug_outlines;
        opts.scroll_y = scroll_y;
        opts.viewport = viewport;

        Renderer::DisplayListRecorder recorder(display_list_, graphics);
        painter_.paint(*render_tree, recorder, opts);

        display_list_valid_ = true;
        display_list_owner_ = render_tree;
        display_list_viewport_ = viewport;
        display_list_scroll_y_ = scroll_y;
        display_list_debug_outlines_ = debug_outlines;
    } else {
        static int reuse_log_counter = 0;
        ++reuse_log_counter;
        HB_LOG_DEBUG("[perf] display list reused commands=" << display_list_.size() << " count=" << reuse_log_counter);
    }

    display_list_.replay(graphics);
    input_controller.paint_controls(render_tree, graphics, viewport, scroll_y, false);
}

void DocumentPainter::invalidate_display_list() {
    display_list_valid_ = false;
    display_list_owner_ = nullptr;
}

bool DocumentPainter::can_reuse_display_list(const Layout::RenderObject* render_tree, const Layout::Rect& viewport,
                                             bool debug_outlines, float scroll_y) const {
    if (!display_list_valid_) {
        return false;
    }
    if (display_list_owner_ != render_tree) {
        return false;
    }
    if (!rect_equals(display_list_viewport_, viewport)) {
        return false;
    }
    if (display_list_scroll_y_ != scroll_y) {
        return false;
    }
    if (display_list_debug_outlines_ != debug_outlines) {
        return false;
    }
    return true;
}

}  // namespace Hummingbird::Engine
