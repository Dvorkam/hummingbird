#pragma once

#include "style/compute/apply/PropertyApplier.h"

namespace Hummingbird::Css::Apply {

bool apply_text_property(Property property, const Value& value, ComputedStyle& style,
                         StyleDefaults::StyleOverrides& overrides, Context& context);

}  // namespace Hummingbird::Css::Apply
