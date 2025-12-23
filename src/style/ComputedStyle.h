#pragma once

#include <optional>

#include "core/IGraphicsContext.h"

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
    Color color{0, 0, 0, 255};
    bool underline = false;
    bool font_monospace = false;
    enum class WhiteSpace { Normal, Preserve };
    WhiteSpace whitespace = WhiteSpace::Normal;
    enum class FontWeight { Normal, Bold };
    enum class FontStyle { Normal, Italic };
    FontWeight weight = FontWeight::Normal;
    FontStyle style = FontStyle::Normal;
    float font_size = 16.0f;  // px
    std::optional<Color> background;
    // Future: background, font family, etc.
};

inline ComputedStyle default_computed_style() {
    return ComputedStyle{};
}

}  // namespace Hummingbird::Css
