#pragma once

#include <optional>
#include <string_view>
#include <vector>

#include "style/compute/Stylesheet.h"

namespace Hummingbird::Css::StyleValueUtils {

// Parses the raw text a var() substitution produced into a typed Value:
// color, length ("4px", "70%"), bare number, or identifier as the fallback.
Value parse_substituted_value(std::string_view text);

struct LengthResolutionContext {
    float font_size = 16.0f;
    float root_font_size = 16.0f;
};

float value_to_length(const Value& value, float fallback, const LengthResolutionContext& context);
std::optional<float> parse_length_token(std::string_view token, const LengthResolutionContext& context);
std::optional<float> parse_length_or_number(std::string_view token, const LengthResolutionContext& context);
std::optional<Unit> parse_unit_token(std::string_view token);
std::vector<std::string_view> split_tokens(std::string_view text);

}  // namespace Hummingbird::Css::StyleValueUtils
