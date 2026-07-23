#include "layout/RenderObject.h"

#include <optional>

#include "core/platform_api/IGraphicsContext.h"
#include "core/platform_api/IImageDecoder.h"
#include "layout/paint/PaintUtils.h"
#include "style/types/ComputedStyle.h"

namespace Hummingbird::Layout {

void RenderObject::layout(IGraphicsContext& context, const Rect& bounds) {
    m_rect = bounds;
}

void RenderObject::paint(IGraphicsContext& context, const Point& offset) const {
    const auto* style = get_computed_style();
    Point paint_offset = offset;
    if (style && style->transform_has_translate) {
        paint_offset.x += style->transform_translate_x;
        paint_offset.y += style->transform_translate_y;
    }
    paint_self(context, paint_offset);

    // overflow: hidden clips descendants to this box (story 8.5.3). We clip only
    // when both axes are Hidden — the `overflow: hidden` shorthand — because a
    // single-axis clip needs an unbounded clip on the visible axis, which
    // push_clip's rectangle cannot express; over-clipping the common
    // `overflow-x: hidden` idiom would hide content the page means to keep. The
    // box's own border/background already painted above, so only content clips.
    // Scroll/Auto are not clipped here: without scroll offsets (M10) that would
    // hide content the user cannot reach.
    const bool clip_children = style && style->overflow_x == Css::ComputedStyle::Overflow::Hidden &&
                               style->overflow_y == Css::ComputedStyle::Overflow::Hidden;
    if (clip_children) {
        // Clip at the padding box (inside the border), which is where CSS overflow
        // clips — the border itself, painted above, stays visible.
        const float bl = style->border_width.left;
        const float bt = style->border_width.top;
        const float br = style->border_width.right;
        const float bb = style->border_width.bottom;
        context.push_clip(Rect{paint_offset.x + m_rect.x + bl, paint_offset.y + m_rect.y + bt, m_rect.width - bl - br,
                               m_rect.height - bt - bb});
    }
    Point child_offset = {paint_offset.x + m_rect.x, paint_offset.y + m_rect.y};
    for (auto& child : m_children) {
        child->paint(context, child_offset);
    }
    if (clip_children) {
        context.pop_clip();
    }
}

void RenderObject::paint_self(IGraphicsContext& context, const Point& offset) const {
    const auto* style = get_computed_style();
    Layout::Rect rect{offset.x + m_rect.x, offset.y + m_rect.y, m_rect.width, m_rect.height};
    PaintUtils::draw_box_decoration(context, rect, style, m_background_image);
}

}  // namespace Hummingbird::Layout
