#pragma once

#include "layout/RenderObject.h"

namespace Hummingbird::Layout {

class BlockBox : public RenderObject {
public:
    using RenderObject::RenderObject;  // Inherit constructor

    void layout(IGraphicsContext& context, const Rect& bounds) override;
    void paint(IGraphicsContext& context, const Point& offset) override;
};

}  // namespace Hummingbird::Layout
