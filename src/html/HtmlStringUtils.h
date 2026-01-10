#pragma once

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

#include "html/HtmlToken.h"

namespace Hummingbird::Html::Utils {

inline std::string to_lower(std::string_view view) {
    std::string out(view);
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

inline bool iequals(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

inline std::string_view find_attribute(const StartTagToken& tag_data, std::string_view name) {
    for (size_t i = 0; i < tag_data.attribute_count; ++i) {
        const auto& attr = tag_data.attributes[i];
        if (iequals(attr.name, name)) {
            return attr.value;
        }
    }
    return {};
}

}  // namespace Hummingbird::Html::Utils
