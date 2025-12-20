#pragma once

// Forward declarations
namespace Hummingbird::Layout {
class RenderObject;
}
class IGraphicsContext;

namespace Hummingbird::Renderer {

class Painter {
public:
    void paint(Layout::RenderObject& root, IGraphicsContext& context);
};

}  // namespace Hummingbird::Renderer