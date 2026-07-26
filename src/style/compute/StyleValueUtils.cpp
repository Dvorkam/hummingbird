#include "style/compute/StyleValueUtils.h"

#include <cctype>
#include <string>

#include "core/utils/ColorUtils.h"
#include "core/utils/ParseUtils.h"
#include "core/utils/StringUtils.h"
#include "style/registry/CssValueNames.h"

namespace Hummingbird::Css::StyleValueUtils {

Value parse_substituted_value(std::string_view text) {
    auto trimmed = Core::Utils::trim_ascii_whitespace(text);
    if (trimmed.empty()) {
        return Value::identifier("");
    }
    if (auto color = Core::Utils::parse_html_color(trimmed)) {
        return Value::color_value(*color);
    }
    // <number>[<unit>|%]: split the numeric prefix from a unit suffix.
    size_t split = 0;
    while (split < trimmed.size()) {
        const char c = trimmed[split];
        const bool sign = (c == '+' || c == '-') && split == 0;
        if (!sign && c != '.' && !std::isdigit(static_cast<unsigned char>(c))) {
            break;
        }
        ++split;
    }
    if (split > 0) {
        if (auto number = Core::Utils::parse_float(trimmed.substr(0, split))) {
            auto suffix = trimmed.substr(split);
            if (suffix.empty()) {
                return Value::number_value(*number);
            }
            if (auto unit = parse_unit_token(suffix)) {
                return Value::length_value(*number, *unit);
            }
        }
    }
    return Value::identifier(std::string(trimmed));
}

float value_to_length(const Value& value, float fallback, const LengthResolutionContext& context) {
    // A unitless number is a valid length only for 0, but treat any unitless
    // value as px (lenient, matches apply_optional_length). Without this,
    // `padding: 0` — a unitless Number — returned the fallback (the current
    // value), so it silently failed to override a UA default.
    if (value.type == Value::Type::Number) {
        return value.number;
    }
    if (value.type != Value::Type::Length) {
        return fallback;
    }
    if (value.length.unit == Unit::Px) {
        return value.length.value;
    }
    if (value.length.unit == Unit::Em) {
        return value.length.value * context.font_size;
    }
    if (value.length.unit == Unit::Rem) {
        return value.length.value * context.root_font_size;
    }
    return fallback;
}

std::optional<float> parse_length_token(std::string_view token, const LengthResolutionContext& context) {
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
    if (token.size() > 3 && token.ends_with(ValueNames::Rem)) {
        auto value = Core::Utils::parse_float(token.substr(0, token.size() - 3));
        if (value) {
            return *value * context.root_font_size;
        }
        return std::nullopt;
    }
    if (token.size() > 2 && token.ends_with(ValueNames::Em)) {
        auto value = Core::Utils::parse_float(token.substr(0, token.size() - 2));
        if (value) {
            return *value * context.font_size;
        }
        return std::nullopt;
    }
    if (token.size() > 1 && token.ends_with("%")) {
        auto value = Core::Utils::parse_float(token.substr(0, token.size() - 1));
        if (value) {
            return *value;
        }
        return std::nullopt;
    }
    return std::nullopt;
}

std::optional<float> parse_length_or_number(std::string_view token, const LengthResolutionContext& context) {
    if (auto length = parse_length_token(token, context)) {
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
    if (token == ValueNames::Rem) {
        return Unit::Rem;
    }
    if (token == "%") {
        return Unit::Percent;
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
