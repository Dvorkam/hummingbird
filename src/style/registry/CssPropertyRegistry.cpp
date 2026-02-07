#include "style/registry/CssPropertyRegistry.h"

#include "style/registry/CssPropertyList.h"

namespace Hummingbird::Css::PropertyRegistry {

static constexpr PropertyEntry kEntries[] = {
#define HB_CSS_PROPERTY(property_id, name_id, css_name, canonical_name, parser_hook, applier_hook, flags) \
    {css_name, Property::property_id},
#define HB_CSS_PROPERTY_ALIAS(property_id, name_id, css_name, canonical_name, parser_hook, applier_hook, flags) \
    {css_name, Property::property_id},
#include "style/registry/CssPropertyList.inl"
#undef HB_CSS_PROPERTY_ALIAS
#undef HB_CSS_PROPERTY
};

Property parse_property_name(std::string_view name) {
    for (const auto& entry : kEntries) {
        if (name == entry.name) {
            return entry.property;
        }
    }
    return Property::Unknown;
}

bool is_supported_property(std::string_view name) {
    return parse_property_name(name) != Property::Unknown;
}

std::span<const PropertyEntry> entries() {
    return kEntries;
}

std::string_view canonical_property_name(Property property) {
    for (const auto& entry : property_list()) {
        if (entry.property == property) {
            return entry.canonical_name;
        }
    }
    return {};
}

}  // namespace Hummingbird::Css::PropertyRegistry
