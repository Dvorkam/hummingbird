#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "style/types/ComputedStyle.h"

namespace Hummingbird::Css::Apply {

// Resolves a `var(--x[, fallback])` expression to the custom property's raw
// text (nested var() in the value or fallback is followed). nullopt when the
// expression is malformed or nothing resolves.
std::optional<std::string> resolve_var_text(const ComputedStyle& style, const ComputedStyle* parent_style,
                                            std::string_view value);

std::optional<Color> resolve_var_color(const ComputedStyle& style, const ComputedStyle* parent_style,
                                       std::string_view value);

}  // namespace Hummingbird::Css::Apply
