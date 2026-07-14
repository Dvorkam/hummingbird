#include "layout/block/FloatLayoutUtils.h"

#include <algorithm>

namespace Hummingbird::Layout::FloatLayout {

bool overlaps_vertical(const Rect& rect, float y, float height) {
    return y < rect.y + rect.height && y + height > rect.y;
}

float clearance_y(const std::vector<FloatBox>& floats, Css::ComputedStyle::Clear clear, float current_y) {
    if (clear == Css::ComputedStyle::Clear::None) {
        return current_y;
    }
    float target = current_y;
    for (const auto& f : floats) {
        const bool clears_side =
            clear == Css::ComputedStyle::Clear::Both ||
            (clear == Css::ComputedStyle::Clear::Left && f.side == Css::ComputedStyle::Float::Left) ||
            (clear == Css::ComputedStyle::Clear::Right && f.side == Css::ComputedStyle::Float::Right);
        if (clears_side) {
            target = std::max(target, f.rect.y + f.rect.height);
        }
    }
    return target;
}

FloatBand compute_float_band(const std::vector<FloatBox>& floats, float y, float height, float content_left,
                             float content_right) {
    FloatBand band{content_left, content_right, y, false};
    for (const auto& f : floats) {
        if (!overlaps_vertical(f.rect, y, height)) {
            continue;
        }
        band.has_overlap = true;
        band.clear_y = std::max(band.clear_y, f.rect.y + f.rect.height);
        if (f.side == Css::ComputedStyle::Float::Left) {
            band.left = std::max(band.left, f.rect.x + f.rect.width);
        } else if (f.side == Css::ComputedStyle::Float::Right) {
            band.right = std::min(band.right, f.rect.x);
        }
    }
    if (band.right < band.left) {
        band.right = band.left;
    }
    return band;
}

FloatPlacement place_float(const std::vector<FloatBox>& floats, Css::ComputedStyle::Float side, float start_y,
                           float box_width, float box_height, float margin_left, float margin_right, float margin_top,
                           float margin_bottom, float content_left, float content_right) {
    FloatPlacement placement;
    float float_y = start_y + margin_top;
    float total_height = box_height + margin_top + margin_bottom;
    float required_width = box_width + margin_left + margin_right;

    FloatBand band = compute_float_band(floats, float_y, total_height, content_left, content_right);
    while (band.has_overlap && (band.right - band.left) < required_width && band.clear_y > float_y) {
        float_y = band.clear_y;
        band = compute_float_band(floats, float_y, total_height, content_left, content_right);
    }

    float float_x = content_left;
    if (side == Css::ComputedStyle::Float::Left) {
        float_x = band.left + margin_left;
    } else if (side == Css::ComputedStyle::Float::Right) {
        float_x = band.right - margin_right - box_width;
    }

    placement.rect = {float_x, float_y, box_width, box_height};
    placement.margin_rect = {float_x - margin_left, float_y - margin_top, box_width + margin_left + margin_right,
                             box_height + margin_top + margin_bottom};
    placement.band = band;
    return placement;
}

}  // namespace Hummingbird::Layout::FloatLayout
