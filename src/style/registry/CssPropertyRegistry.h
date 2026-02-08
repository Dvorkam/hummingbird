#pragma once

#include <span>
#include <string_view>

#include "style/registry/CssPropertyList.h"

namespace Hummingbird::Css::PropertyRegistry {

struct PropertyEntry {
    std::string_view name;
    Property property;
};

Property parse_property_name(std::string_view name);
bool is_supported_property(std::string_view name);
std::span<const PropertyEntry> entries();
std::string_view canonical_property_name(Property property);
ParserHook parser_hook(Property property);
ApplyHook applier_hook(Property property);

}  // namespace Hummingbird::Css::PropertyRegistry
