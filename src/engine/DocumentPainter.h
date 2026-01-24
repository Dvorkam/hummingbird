#pragma once

#include "core/platform_api/IGraphicsContext.h"
#include "engine/DocumentInputController.h"
#include "layout/Geometry.h"
#include "renderer/Painter.h"

namespace Hummingbird::Layout {
class RenderObject;
}

namespace Hummingbird::Engine {

class DocumentPainter {
public:
    void paint(const Layout::RenderObject* render_tree, IGraphicsContext& graphics, const Layout::Rect& viewport,
               bool debug_outlines, float scroll_y, const DocumentInputController& input_controller);

private:
    Renderer::Painter painter_;
};

}  // namespace Hummingbird::Engine
