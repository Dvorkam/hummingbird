#include "engine/document/DocumentRenderer.h"

#include <algorithm>
#include <cstdlib>

#include "core/platform_api/IGraphicsContext.h"
#include "core/utils/Log.h"
#include "core/utils/Timing.h"
#include "engine/document/DocumentInteraction.h"
#include "engine/document/DocumentModel.h"
#include "layout/RenderObject.h"
#include "layout/geometry/PositioningUtils.h"

namespace Hummingbird::Engine {

namespace {
bool relayout_debug_enabled() {
    static const bool enabled = std::getenv("HB_DEBUG_LAYOUT_STATE") != nullptr;
    return enabled;
}

float compute_max_subtree_bottom(const Layout::RenderObject& node, float parent_abs_y) {
    const auto& rect = node.get_rect();
    float abs_y = parent_abs_y + rect.y;
    float max_bottom = abs_y + rect.height;
    for (const auto& child : node.get_children()) {
        max_bottom = std::max(max_bottom, compute_max_subtree_bottom(*child, abs_y));
    }
    return max_bottom;
}

float compute_content_height(const Layout::RenderObject& root) {
    const auto& root_rect = root.get_rect();
    float max_bottom = compute_max_subtree_bottom(root, 0.0f);
    return std::max(0.0f, max_bottom - root_rect.y);
}
}  // namespace

DocumentRenderer::DocumentRenderer(DocumentModel& model, DocumentInteraction& interaction)
    : model_(model), interaction_(interaction) {}

void DocumentRenderer::reset() {
    content_height_ = 0.0f;
    painter_.invalidate_display_list();
}

void DocumentRenderer::relayout(IGraphicsContext& graphics, const Layout::Rect& viewport) {
    auto* render_tree = model_.render_tree();
    if (!render_tree) return;

    painter_.invalidate_display_list();

    const auto layout_start = Core::Clock::now();
    render_tree->layout(graphics, viewport);
    Layout::Positioning::apply_positioning(*render_tree, graphics, viewport);
    const auto layout_end = Core::Clock::now();
    content_height_ = compute_content_height(*render_tree);
    HB_LOG_INFO("[perf] layout ms=" << Core::duration_ms(layout_start, layout_end) << " viewport=" << viewport.width
                                    << "x" << viewport.height);
    if (relayout_debug_enabled()) {
        const auto& root = render_tree->get_rect();
        HB_LOG_WARN("[layout-debug] relayout root_rect=(" << root.x << "," << root.y << "," << root.width << ","
                                                          << root.height << ") content_h=" << content_height_);
    }
}

void DocumentRenderer::paint(IGraphicsContext& graphics, const PaintContext& context) {
    if (!model_.render_tree()) return;

    const auto paint_start = Core::Clock::now();
    painter_.paint(model_.render_tree(), graphics, context.viewport, context.debug_outlines, context.scroll_y,
                   interaction_.input_controller());
    const auto paint_end = Core::Clock::now();
    static int paint_log_counter = 0;
    if (++paint_log_counter % 5 == 0) {
        HB_LOG_DEBUG("[perf] paint ms=" << Core::duration_ms(paint_start, paint_end)
                                        << " scroll_y=" << context.scroll_y);
    }
}

void DocumentRenderer::paint_controls(IGraphicsContext& graphics, const PaintContext& context,
                                      bool repaint_background) {
    auto* render_tree = model_.render_tree();
    if (!render_tree) return;

    graphics.set_viewport(context.viewport);
    interaction_.paint_controls(render_tree, graphics, context.viewport, context.scroll_y, repaint_background);
}

}  // namespace Hummingbird::Engine
