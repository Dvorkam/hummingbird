#pragma once

#include <optional>
#include <string_view>

#include "core/dom/Element.h"

namespace Hummingbird::DOM {

inline std::optional<std::string_view> find_attribute_value(const Element& element, std::string_view name) {
    if (const auto* value = element.find_attribute(name)) {
        return std::string_view(*value);
    }
    return std::nullopt;
}

}  // namespace Hummingbird::DOM
