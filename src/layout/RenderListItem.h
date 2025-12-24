#pragma once

#include <memory>

#include "layout/BlockBox.h"

namespace Hummingbird::Layout {

class RenderMarker;

class RenderListItem : public BlockBox {
public:
    explicit RenderListItem(const DOM::Node* dom_node);

    const Rect& marker_rect() const;

    void layout(IGraphicsContext& context, const Rect& bounds) override;
    void paint_self(IGraphicsContext& context, const Point& offset) override;

private:
    std::unique_ptr<RenderMarker> m_marker;
};

class RenderMarker : public RenderObject {
public:
    using RenderObject::RenderObject;

    void layout(IGraphicsContext& context, const Rect& bounds) override;
    void paint(IGraphicsContext& context, const Point& offset) override;

private:
    float m_size = 6.0f;
};

}  // namespace Hummingbird::Layout
