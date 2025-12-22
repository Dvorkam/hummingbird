#pragma once

#include "layout/RenderObject.h"

namespace Hummingbird::Layout {

class InlineBox : public RenderObject {
public:
    using RenderObject::RenderObject;

    bool is_inline() const override { return true; }

    void layout(IGraphicsContext& context, const Rect& bounds) override;
    void paint(IGraphicsContext& context, const Point& offset) override;
};

}  // namespace Hummingbird::Layout
