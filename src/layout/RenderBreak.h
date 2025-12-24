#pragma once

#include "layout/RenderObject.h"

namespace Hummingbird::Layout {

class RenderBreak : public RenderObject {
public:
    using RenderObject::RenderObject;
    void layout(IGraphicsContext& context, const Rect& bounds) override;
    void paint_self(IGraphicsContext& context, const Point& offset) override;
};

}  // namespace Hummingbird::Layout
