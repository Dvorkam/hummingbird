#include "style/compute/StyleValueUtils.h"

#include "core/utils/ParseUtils.h"
#include "core/utils/StringUtils.h"
#include "style/registry/CssValueNames.h"

namespace Hummingbird::Css::StyleValueUtils {

float value_to_length(const Value& value, float fallback, float font_size) {
    if (value.type != Value::Type::Length) {
        return fallback;
    }
    if (value.length.unit == Unit::Px) {
        return value.length.value;
    }
    if (value.length.unit == Unit::Em) {
        return value.length.value * font_size;
    }
    return fallback;
}

std::optional<float> parse_length_token(std::string_view token, float font_size) {
    token = Core::Utils::trim_ascii_whitespace(token);
    if (token.empty()) {
        return std::nullopt;
    }
    if (token.size() > 2 && token.ends_with(ValueNames::Px)) {
        auto value = Core::Utils::parse_float(token.substr(0, token.size() - 2));
        if (value) {
            return *value;
        }
        return std::nullopt;
    }
    if (token.size() > 2 && token.ends_with(ValueNames::Em)) {
        auto value = Core::Utils::parse_float(token.substr(0, token.size() - 2));
        if (value) {
            return *value * font_size;
        }
        return std::nullopt;
    }
    return std::nullopt;
}

std::optional<float> parse_length_or_number(std::string_view token, float font_size) {
    if (auto length = parse_length_token(token, font_size)) {
        return *length;
    }
    if (auto value = Core::Utils::parse_float(token)) {
        return *value;
    }
    return std::nullopt;
}

std::optional<Unit> parse_unit_token(std::string_view token) {
    if (token == ValueNames::Px) {
        return Unit::Px;
    }
    if (token == ValueNames::Em) {
        return Unit::Em;
    }
    return std::nullopt;
}

std::vector<std::string_view> split_tokens(std::string_view text) {
    std::vector<std::string_view> tokens;
    size_t start = 0;
    while (start < text.size()) {
        start = text.find_first_not_of(" \t\r\n", start);
        if (start == std::string_view::npos) {
            break;
        }
        size_t end = text.find_first_of(" \t\r\n", start);
        if (end == std::string_view::npos) {
            tokens.push_back(text.substr(start));
            break;
        }
        tokens.push_back(text.substr(start, end - start));
        start = end;
    }
    return tokens;
}

}  // namespace Hummingbird::Css::StyleValueUtils
