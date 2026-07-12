#pragma once

#include "style/compute/StyleDefaults.h"
#include "style/compute/Stylesheet.h"
#include "style/types/ComputedStyle.h"

namespace Hummingbird::Css::Apply {

struct Context {
    float parent_font_size = 0.0f;
    const ComputedStyle* parent_style = nullptr;
    bool* display_set = nullptr;
};

void apply_property(Property property, const Value& value, ComputedStyle& style,
                    StyleDefaults::StyleOverrides& overrides, Context& context);

}  // namespace Hummingbird::Css::Apply
