#pragma once

#include "layout/RenderObject.h"

namespace Hummingbird::Layout {

class RenderRule : public RenderObject {
public:
    using RenderObject::RenderObject;
    void layout(IGraphicsContext& context, const Rect& bounds) override;
    void paint(IGraphicsContext& context, const Point& offset) override;
};

} // namespace Hummingbird::Layout
