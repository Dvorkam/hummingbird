#include "renderer/Painter.h"

#include "core/IGraphicsContext.h"
#include "layout/RenderObject.h"

namespace Hummingbird::Renderer {

void Painter::paint(Layout::RenderObject& root, IGraphicsContext& context) {
    // Start the recursive paint process from the root with no offset.
    root.paint(context, {0, 0});
}

}  // namespace Hummingbird::Renderer