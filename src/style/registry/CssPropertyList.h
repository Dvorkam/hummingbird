#pragma once

#include <cstdint>
#include <span>
#include <string_view>

#include "style/compute/Stylesheet.h"

namespace Hummingbird::Css::PropertyRegistry {

enum class PropertyFlags : std::uint8_t {
    None = 0,
    Inherited = 1 << 0,
    LayoutAffecting = 1 << 1,
};

constexpr PropertyFlags operator|(PropertyFlags lhs, PropertyFlags rhs) {
    return static_cast<PropertyFlags>(static_cast<std::uint8_t>(lhs) | static_cast<std::uint8_t>(rhs));
}

constexpr bool has_flag(PropertyFlags value, PropertyFlags flag) {
    return (static_cast<std::uint8_t>(value) & static_cast<std::uint8_t>(flag)) != 0;
}

struct PropertyListEntry {
    Property property;
    std::string_view name;
    std::string_view canonical_name;
    std::string_view parser_hook;
    std::string_view applier_hook;
    PropertyFlags flags = PropertyFlags::None;
};

// Phase 4/R4-01: central metadata list for CSS properties.
// Existing parser/apply paths are unchanged for now; this file is additive.
inline constexpr PropertyListEntry kPropertyList[] = {
#define HB_CSS_PROPERTY(property_id, name_id, css_name, canonical_name, parser_hook, applier_hook, flags) \
    {Property::property_id, css_name, canonical_name, parser_hook, applier_hook, flags},
#define HB_CSS_PROPERTY_ALIAS(property_id, name_id, css_name, canonical_name, parser_hook, applier_hook, flags) \
    {Property::property_id, css_name, canonical_name, parser_hook, applier_hook, flags},
#include "style/registry/CssPropertyList.inl"
#undef HB_CSS_PROPERTY_ALIAS
#undef HB_CSS_PROPERTY
};

inline std::span<const PropertyListEntry> property_list() {
    return kPropertyList;
}

}  // namespace Hummingbird::Css::PropertyRegistry
