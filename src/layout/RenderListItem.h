#pragma once

#include <memory>

#include "layout/BlockBox.h"

class ListItemLayoutTest_GeneratesMarkerLeftOfContent_Test;
class PainterTest_PaintsListMarkersWithCulling_Test;

namespace Hummingbird::Layout {

class RenderMarker;

inline constexpr float kListMarkerSizePx = 6.0f;
inline constexpr float kListMarkerGapPx = 6.0f;

class RenderListItem : public BlockBox {
public:
    explicit RenderListItem(const DOM::Node* dom_node);

    void layout(IGraphicsContext& context, const Rect& bounds) override;
    void paint_self(IGraphicsContext& context, const Point& offset) override;

private:
    friend class ::ListItemLayoutTest_GeneratesMarkerLeftOfContent_Test;
    friend class ::PainterTest_PaintsListMarkersWithCulling_Test;

    const Rect& marker_rect() const;

    std::unique_ptr<RenderMarker> m_marker;
};

class RenderMarker : public RenderObject {
public:
    using RenderObject::RenderObject;

    void layout(IGraphicsContext& context, const Rect& bounds) override;
    void paint_self(IGraphicsContext& context, const Point& offset) override;

private:
    float m_size = kListMarkerSizePx;
};

}  // namespace Hummingbird::Layout
