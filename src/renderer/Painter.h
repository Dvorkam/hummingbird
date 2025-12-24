#pragma once

#include "core/platform_api/IGraphicsContext.h"
#include "layout/RenderObject.h"

namespace Hummingbird::Renderer {

struct PaintOptions {
    bool debug_outlines = false;
    float scroll_y = 0.0f;
    Layout::Rect viewport{0, 0, 0, 0};
};

class Painter {
public:
    void paint(Layout::RenderObject& root, IGraphicsContext& context, const PaintOptions& options = PaintOptions{});
};

}  // namespace Hummingbird::Renderer
