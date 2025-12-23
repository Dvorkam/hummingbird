#pragma once

#include "layout/RenderObject.h"

namespace Hummingbird::Layout {

class BlockBox : public RenderObject {
public:
    using RenderObject::RenderObject;  // Inherit constructor

    void layout(IGraphicsContext& context, const Rect& bounds) override;
    void paint(IGraphicsContext& context, const Point& offset) override;
};

class InlineBlockBox : public BlockBox {
public:
    using BlockBox::BlockBox;

    bool is_inline() const override { return true; }
    void layout(IGraphicsContext& context, const Rect& bounds) override;
};

}  // namespace Hummingbird::Layout
