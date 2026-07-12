#pragma once

#include <optional>
#include <string_view>

#include "style/types/ComputedStyle.h"

namespace Hummingbird::Css::Apply {

std::optional<Color> resolve_var_color(const ComputedStyle& style, const ComputedStyle* parent_style,
                                       std::string_view value);

}  // namespace Hummingbird::Css::Apply
