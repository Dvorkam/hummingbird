#pragma once

#include "layout/Geometry.h"

namespace Hummingbird::Layout {

inline bool rect_contains_point(const Rect& rect, const Point& point) {
    if (rect.width <= 0.0f || rect.height <= 0.0f) return false;
    return point.x >= rect.x && point.x <= rect.x + rect.width && point.y >= rect.y && point.y <= rect.y + rect.height;
}

inline bool rect_intersects(const Rect& a, const Rect& b) {
    if (a.width <= 0.0f || a.height <= 0.0f) return false;
    if (b.width <= 0.0f || b.height <= 0.0f) return false;
    return !(a.x + a.width <= b.x || a.x >= b.x + b.width || a.y + a.height <= b.y || a.y >= b.y + b.height);
}

}  // namespace Hummingbird::Layout
