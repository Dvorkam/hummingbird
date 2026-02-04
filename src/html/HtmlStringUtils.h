#pragma once

#include <cctype>
#include <string>
#include <string_view>

#include "core/utils/StringUtils.h"
#include "html/HtmlToken.h"

namespace Hummingbird::Html::Utils {

inline std::string_view find_attribute(const StartTagToken& tag_data, std::string_view name) {
    for (const auto& attr : tag_data.attributes) {
        if (Core::Utils::equals_ignore_case(attr.name, name)) {
            return attr.value;
        }
    }
    return {};
}

inline bool is_html_whitespace(char ch) {
    return std::isspace(static_cast<unsigned char>(ch)) != 0;
}

inline bool is_tag_name_char(char ch) {
    const unsigned char uch = static_cast<unsigned char>(ch);
    return std::isalnum(uch) != 0 || ch == ':' || ch == '-';
}

inline bool is_attr_name_char(char ch) {
    const unsigned char uch = static_cast<unsigned char>(ch);
    return std::isalnum(uch) != 0 || ch == '-' || ch == '_' || ch == ':';
}

}  // namespace Hummingbird::Html::Utils
