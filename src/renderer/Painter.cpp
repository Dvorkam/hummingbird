#include "renderer/Painter.h"

#include <algorithm>
#include <vector>

#include "core/platform_api/IGraphicsContext.h"
#include "layout/RenderObject.h"
#include "layout/geometry/GeometryUtils.h"
#include "layout/geometry/PositioningUtils.h"
#include "layout/paint/PaintUtils.h"
#include "renderer/RenderCommandUtils.h"

namespace Hummingbird::Renderer {

namespace {
constexpr Color kOutlineColor{255, 0, 0, 100};

struct PaintContext {
    Layout::Point offset;
    const Layout::Rect* viewport = nullptr;
    bool debug_outlines = false;
    std::vector<float> alpha_stack;
};

void paint_tree(const Layout::RenderObject& node, IGraphicsContext& context, const PaintContext& paint_context) {
    PaintContext mutable_context = paint_context;
    mutable_context.alpha_stack.push_back(1.0f);
    context.set_global_alpha(1.0f);
    Layout::Positioning::traverse_render_tree_z_order(
        node, mutable_context.offset,
        [&](const Layout::RenderObject& current, const Layout::Rect& absolute, const Layout::Point& local_offset) {
            if (mutable_context.viewport && !Layout::rect_intersects(absolute, *mutable_context.viewport) &&
                !current.has_absolute_descendant()) {
                return Layout::Traversal::TraverseAction::SkipChildren;
            }
            float local_opacity = 1.0f;
            if (const auto* style = current.get_computed_style()) {
                local_opacity = std::clamp(style->opacity, 0.0f, 1.0f);
            }
            const float parent_opacity = mutable_context.alpha_stack.back();
            const float node_opacity = parent_opacity * local_opacity;
            mutable_context.alpha_stack.push_back(node_opacity);
            context.set_global_alpha(node_opacity);
            current.paint_self(context, local_offset);
            if (mutable_context.debug_outlines) {
                RenderCommandUtils::draw_outline(context, absolute, kOutlineColor);
            }
            return Layout::Traversal::TraverseAction::Continue;
        },
        [&](const Layout::RenderObject&, const Layout::Rect&, const Layout::Point&) {
            if (!mutable_context.alpha_stack.empty()) {
                mutable_context.alpha_stack.pop_back();
            }
            const float restore_alpha = mutable_context.alpha_stack.empty() ? 1.0f : mutable_context.alpha_stack.back();
            context.set_global_alpha(restore_alpha);
            return Layout::Traversal::TraverseAction::Continue;
        });
    context.set_global_alpha(1.0f);
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
