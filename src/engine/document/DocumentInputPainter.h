#pragma once

#include <cstddef>

#include "core/platform_api/IGraphicsContext.h"
#include "layout/geometry/Geometry.h"

namespace Hummingbird::DOM {
class Element;
}

namespace Hummingbird::Layout {
class RenderObject;
}

namespace Hummingbird::Engine {

void paint_input_control(const DOM::Element& element, const Layout::RenderObject& node, const Layout::Rect& absolute,
                         const Layout::Point& local_offset, IGraphicsContext& graphics, bool repaint_background,
                         bool focused, size_t caret, float scroll_y);

}  // namespace Hummingbird::Engine
