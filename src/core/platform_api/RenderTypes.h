#pragma once

#include <string>

namespace Hummingbird {

struct Color {
    unsigned char r, g, b, a;
};

struct TextMetrics {
    float width = 0.0f;
    float height = 0.0f;
    float ascent = 0.0f;
    float descent = 0.0f;
    float underline_position = 0.0f;
    float underline_thickness = 0.0f;
};

struct TextStyle {
    std::string font_path;
    float font_size = 16.0f;
    bool bold = false;
    bool italic = false;
    bool monospace = false;
    Color color{0, 0, 0, 255};
};

}  // namespace Hummingbird
