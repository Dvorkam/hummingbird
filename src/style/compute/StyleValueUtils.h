#pragma once

#include <optional>
#include <string_view>
#include <vector>

#include "style/compute/Stylesheet.h"

namespace Hummingbird::Css::StyleValueUtils {

float value_to_length(const Value& value, float fallback, float font_size);
std::optional<float> parse_length_token(std::string_view token, float font_size);
std::optional<float> parse_length_or_number(std::string_view token, float font_size);
std::vector<std::string_view> split_tokens(std::string_view text);

}  // namespace Hummingbird::Css::StyleValueUtils
