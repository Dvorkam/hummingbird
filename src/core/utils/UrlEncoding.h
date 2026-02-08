#pragma once

#include <cctype>
#include <string>
#include <string_view>

namespace Hummingbird::Core::Utils {

inline std::string url_encode_component(std::string_view input) {
    static constexpr char kHex[] = "0123456789ABCDEF";
    std::string out;
    out.reserve(input.size());

    for (unsigned char c : input) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            out.push_back(static_cast<char>(c));
            continue;
        }
        out.push_back('%');
        out.push_back(kHex[c >> 4]);
        out.push_back(kHex[c & 0x0F]);
    }

    return out;
}

}  // namespace Hummingbird::Core::Utils
