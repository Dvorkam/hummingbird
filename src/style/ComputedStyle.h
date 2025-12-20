#pragma once

#include <optional>

namespace Hummingbird::Css {

struct EdgeSizes {
    float top = 0;
    float right = 0;
    float bottom = 0;
    float left = 0;
};

struct ComputedStyle {
    EdgeSizes margin;
    EdgeSizes padding;
    std::optional<float> width;
    std::optional<float> height;
    // Future: color, background, etc.
};

inline ComputedStyle default_computed_style() {
    return ComputedStyle{};
}

} // namespace Hummingbird::Css
