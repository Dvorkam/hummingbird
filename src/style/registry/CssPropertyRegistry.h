#pragma once

#include <span>
#include <string_view>

#include "style/compute/Stylesheet.h"

namespace Hummingbird::Css::PropertyRegistry {

struct PropertyEntry {
    std::string_view name;
    Property property;
};

Property parse_property_name(std::string_view name);
bool is_supported_property(std::string_view name);
std::span<const PropertyEntry> entries();

}  // namespace Hummingbird::Css::PropertyRegistry
