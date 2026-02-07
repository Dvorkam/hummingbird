#pragma once

#include "core/platform_api/IGraphicsContext.h"
#include "engine/document/DocumentInputController.h"
#include "layout/geometry/Geometry.h"
#include "renderer/DisplayList.h"
#include "renderer/Painter.h"

namespace Hummingbird::Layout {
class RenderObject;
}

namespace Hummingbird::Engine {

class DocumentPainter {
public:
    void paint(const Layout::RenderObject* render_tree, IGraphicsContext& graphics, const Layout::Rect& viewport,
               bool debug_outlines, float scroll_y, const DocumentInputController& input_controller);
    void invalidate_display_list();
    size_t display_list_generation() const { return display_list_generation_; }

private:
    bool can_reuse_display_list(const Layout::RenderObject* render_tree, const Layout::Rect& viewport,
                                bool debug_outlines, float scroll_y) const;

    Renderer::Painter painter_;
    Renderer::DisplayList display_list_;
    bool display_list_valid_ = false;
    const Layout::RenderObject* display_list_owner_ = nullptr;
    Layout::Rect display_list_viewport_{0, 0, 0, 0};
    float display_list_scroll_y_ = 0.0f;
    bool display_list_debug_outlines_ = false;
    size_t display_list_generation_ = 0;
};

}  // namespace Hummingbird::Engine
