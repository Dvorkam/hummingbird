#pragma once

#include <vector>

#include "layout/geometry/Geometry.h"
#include "style/types/ComputedStyle.h"

namespace Hummingbird::Layout::FloatLayout {

struct FloatBox {
    Rect rect;
    Css::ComputedStyle::Float side = Css::ComputedStyle::Float::None;
};

struct FloatBand {
    float left = 0.0f;
    float right = 0.0f;
    float clear_y = 0.0f;
    bool has_overlap = false;
};

struct FloatPlacement {
    Rect rect;
    Rect margin_rect;
    FloatBand band;
};

inline constexpr float kFloatLineHeightFallback = 16.0f;

bool overlaps_vertical(const Rect& rect, float y, float height);
FloatBand compute_float_band(const std::vector<FloatBox>& floats, float y, float height, float content_left,
                             float content_right);
FloatPlacement place_float(const std::vector<FloatBox>& floats, Css::ComputedStyle::Float side, float start_y,
                           float box_width, float box_height, float margin_left, float margin_right, float margin_top,
                           float margin_bottom, float content_left, float content_right);

}  // namespace Hummingbird::Layout::FloatLayout
