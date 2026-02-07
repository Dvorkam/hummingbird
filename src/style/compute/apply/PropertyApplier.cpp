#include "style/compute/apply/PropertyApplier.h"

#include "style/compute/apply/ApplyBackground.h"
#include "style/compute/apply/ApplyLayout.h"
#include "style/compute/apply/ApplyText.h"

namespace Hummingbird::Css::Apply {

void apply_property(Property property, const Value& value, ComputedStyle& style,
                    StyleDefaults::StyleOverrides& overrides, Context& context) {
    if (apply_layout_property(property, value, style, overrides, context)) {
        return;
    }
    if (apply_text_property(property, value, style, overrides, context)) {
        return;
    }
    (void)apply_background_property(property, value, style, overrides, context);
}

}  // namespace Hummingbird::Css::Apply
