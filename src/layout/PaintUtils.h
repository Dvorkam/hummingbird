#pragma once

#include "core/platform_api/IGraphicsContext.h"
#include "layout/Geometry.h"

namespace Hummingbird::Layout::PaintUtils {

inline void draw_outline(IGraphicsContext& context, const Rect& rect, const Color& color, float thickness = 1.0f) {
    Rect top{rect.x, rect.y, rect.width, thickness};
    Rect bottom{rect.x, rect.y + rect.height - thickness, rect.width, thickness};
    Rect left{rect.x, rect.y, thickness, rect.height};
    Rect right{rect.x + rect.width - thickness, rect.y, thickness, rect.height};
    context.fill_rect(top, color);
    context.fill_rect(bottom, color);
    context.fill_rect(left, color);
    context.fill_rect(right, color);
}

}  // namespace Hummingbird::Layout::PaintUtils
