#pragma once

#include <optional>
#include <string>
#include <unordered_map>

#include "core/platform_api/IGraphicsContext.h"

namespace Hummingbird::Css {

struct EdgeSizes {
    float top = 0;
    float right = 0;
    float bottom = 0;
    float left = 0;
};

struct ComputedStyle {
    enum class Display { Block, Inline, InlineBlock, ListItem, None };
    Display display = Display::Block;
    enum class Float { None, Left, Right };
    Float float_type = Float::None;
    enum class TextAlign { Left, Center, Right };
    TextAlign text_align = TextAlign::Left;
    enum class BorderStyle { None, Solid, Outset, Inset, Ridge, Groove };
    BorderStyle border_style = BorderStyle::None;
    EdgeSizes border_width;
    Color border_color{0, 0, 0, 255};
    EdgeSizes margin;
    bool margin_left_auto = false;
    bool margin_right_auto = false;
    EdgeSizes padding;
    std::optional<float> width;
    std::optional<float> height;
    std::optional<float> max_width;
    Color color{0, 0, 0, 255};
    bool underline = false;
    std::optional<Color> link_color;
    std::optional<Color> vlink_color;
    bool font_monospace = false;
    enum class WhiteSpace { Normal, Preserve, NoWrap };
    WhiteSpace whitespace = WhiteSpace::Normal;
    enum class FontWeight { Normal, Bold };
    enum class FontStyle { Normal, Italic };
    FontWeight weight = FontWeight::Normal;
    FontStyle style = FontStyle::Normal;
    float font_size = 16.0f;   // px
    float line_height = 0.0f;  // px, 0 means use font metrics
    std::string font_face;
    std::optional<Color> background;
    std::unordered_map<std::string, std::string> custom_properties;
    // Future: background, font family, etc.
};

inline ComputedStyle default_computed_style() {
    return ComputedStyle{};
}

}  // namespace Hummingbird::Css
