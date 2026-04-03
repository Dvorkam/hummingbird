#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "core/platform_api/RenderTypes.h"
#include "core/utils/StringUtils.h"

namespace Hummingbird::Core::Utils {

inline std::optional<Color> parse_hex_color(std::string_view hex) {
    auto is_hex_digit = [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    };
    auto hex_value = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
        if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
        return -1;
    };

    if (hex.size() == 3 || hex.size() == 6) {
        for (char c : hex) {
            if (!is_hex_digit(c)) {
                return std::nullopt;
            }
        }
        if (hex.size() == 3) {
            int r = hex_value(hex[0]);
            int g = hex_value(hex[1]);
            int b = hex_value(hex[2]);
            return Color{static_cast<unsigned char>(r * 17), static_cast<unsigned char>(g * 17),
                         static_cast<unsigned char>(b * 17), 255};
        }
        int r1 = hex_value(hex[0]);
        int r2 = hex_value(hex[1]);
        int g1 = hex_value(hex[2]);
        int g2 = hex_value(hex[3]);
        int b1 = hex_value(hex[4]);
        int b2 = hex_value(hex[5]);
        return Color{static_cast<unsigned char>((r1 << 4) + r2), static_cast<unsigned char>((g1 << 4) + g2),
                     static_cast<unsigned char>((b1 << 4) + b2), 255};
    }

    return std::nullopt;
}

inline std::optional<Color> parse_html_color(std::string_view value) {
    std::string_view trimmed = trim_ascii_whitespace(value);
    if (trimmed.empty()) {
        return std::nullopt;
    }
    if (trimmed.front() == '#') {
        trimmed.remove_prefix(1);
    }

    if (auto parsed = parse_hex_color(trimmed)) {
        return parsed;
    }

    std::string lowered = to_lower(trimmed);
    if (lowered == "black") return Color{0, 0, 0, 255};
    if (lowered == "white") return Color{255, 255, 255, 255};
    if (lowered == "red") return Color{255, 0, 0, 255};
    if (lowered == "blue") return Color{0, 0, 255, 255};
    if (lowered == "green") return Color{0, 128, 0, 255};

    return std::nullopt;
}

inline std::string color_to_hex(Color color) {
    auto hex_digit = [](int v) { return static_cast<char>(v < 10 ? '0' + v : 'a' + (v - 10)); };
    std::string out;
    out.reserve(7);
    out.push_back('#');
    out.push_back(hex_digit((color.r >> 4) & 0xF));
    out.push_back(hex_digit(color.r & 0xF));
    out.push_back(hex_digit((color.g >> 4) & 0xF));
    out.push_back(hex_digit(color.g & 0xF));
    out.push_back(hex_digit((color.b >> 4) & 0xF));
    out.push_back(hex_digit(color.b & 0xF));
    return out;
}

}  // namespace Hummingbird::Core::Utils
