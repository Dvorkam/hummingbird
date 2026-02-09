#pragma once

#include <memory>
#include <string>

#include "layout/RenderObject.h"
#include "layout/block/BlockBox.h"

class ListItemLayoutTest_GeneratesMarkerLeftOfContent_Test;
class ListItemLayoutTest_SuppressesMarkerWhenListStyleNone_Test;
class ListItemLayoutTest_UsesWiderMarkerForOrderedList_Test;
class PainterTest_PaintsListMarkersWithCulling_Test;
namespace Hummingbird {
namespace DOM {
class Node;
}  // namespace DOM
}  // namespace Hummingbird

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
    friend class ::ListItemLayoutTest_SuppressesMarkerWhenListStyleNone_Test;
    friend class ::ListItemLayoutTest_UsesWiderMarkerForOrderedList_Test;
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
    void set_disc(float size);
    void set_text(std::string text, float width, float height);

private:
    explicit RenderMarker(const DOM::Node* dom_node) : RenderObject(dom_node) {}

    enum class Kind { Disc, Text };
    Kind m_kind = Kind::Disc;
    float m_size = kListMarkerSizePx;
    float m_text_width = 0.0f;
    float m_text_height = 0.0f;
    std::string m_text;
};

}  // namespace Hummingbird::Layout
