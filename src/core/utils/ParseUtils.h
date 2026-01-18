#pragma once

#include <cctype>
#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>

#include "core/utils/StringUtils.h"

namespace Hummingbird::Core::Utils {

enum class NumberParseMode {
    Strict,
    AllowTrailing,
};

inline std::optional<float> parse_float(std::string_view input, NumberParseMode mode = NumberParseMode::Strict) {
    std::string_view trimmed = trim_ascii_whitespace(input);
    if (trimmed.empty()) {
        return std::nullopt;
    }

    std::string temp(trimmed);
    char* end = nullptr;
    float parsed = std::strtof(temp.c_str(), &end);
    if (end == temp.c_str()) {
        return std::nullopt;
    }
    if (mode == NumberParseMode::Strict) {
        while (end && *end != '\0' && std::isspace(static_cast<unsigned char>(*end))) {
            ++end;
        }
        if (end && *end != '\0') {
            return std::nullopt;
        }
    }
    return parsed;
}

inline std::optional<long> parse_long(std::string_view input, NumberParseMode mode = NumberParseMode::Strict) {
    std::string_view trimmed = trim_ascii_whitespace(input);
    if (trimmed.empty()) {
        return std::nullopt;
    }

    std::string temp(trimmed);
    char* end = nullptr;
    long parsed = std::strtol(temp.c_str(), &end, 10);
    if (end == temp.c_str()) {
        return std::nullopt;
    }
    if (mode == NumberParseMode::Strict) {
        while (end && *end != '\0' && std::isspace(static_cast<unsigned char>(*end))) {
            ++end;
        }
        if (end && *end != '\0') {
            return std::nullopt;
        }
    }
    return parsed;
}

}  // namespace Hummingbird::Core::Utils
