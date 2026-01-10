#pragma once

#include <string>
#include <string_view>

#include "core/utils/StringUtils.h"
#include "html/HtmlToken.h"

namespace Hummingbird::Html::Utils {

inline std::string_view find_attribute(const StartTagToken& tag_data, std::string_view name) {
    for (const auto& attr : tag_data.attributes) {
        if (Core::iequals(attr.name, name)) {
            return attr.value;
        }
    }
    return {};
}

}  // namespace Hummingbird::Html::Utils
