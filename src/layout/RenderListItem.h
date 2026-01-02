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
    static std::unique_ptr<RenderListItem> create(const DOM::Node* dom_node) {
        return std::unique_ptr<RenderListItem>(new RenderListItem(dom_node));
    }

    void layout(IGraphicsContext& context, const Rect& bounds) override;
    void paint_self(IGraphicsContext& context, const Point& offset) const override;

private:
    friend class ::ListItemLayoutTest_GeneratesMarkerLeftOfContent_Test;
    friend class ::PainterTest_PaintsListMarkersWithCulling_Test;

    explicit RenderListItem(const DOM::Node* dom_node);

    const Rect& marker_rect() const;

    std::unique_ptr<RenderMarker> m_marker;
};

class RenderMarker : public RenderObject {
public:
    static std::unique_ptr<RenderMarker> create(const DOM::Node* dom_node) {
        return std::unique_ptr<RenderMarker>(new RenderMarker(dom_node));
    }

    void layout(IGraphicsContext& context, const Rect& bounds) override;
    void paint_self(IGraphicsContext& context, const Point& offset) const override;

private:
    explicit RenderMarker(const DOM::Node* dom_node) : RenderObject(dom_node) {}

    float m_size = kListMarkerSizePx;
};

}  // namespace Hummingbird::Layout
