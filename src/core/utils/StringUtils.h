#pragma once

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>
#include <vector>

namespace Hummingbird::Core::Utils {

inline std::string to_lower(std::string_view input) {
    std::string out(input);
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

inline std::string to_upper(std::string_view input) {
    std::string out(input);
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return out;
}

inline bool equals_ignore_case(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

inline std::string_view trim_ascii_whitespace(std::string_view input) {
    while (!input.empty() && std::isspace(static_cast<unsigned char>(input.front()))) {
        input.remove_prefix(1);
    }
    while (!input.empty() && std::isspace(static_cast<unsigned char>(input.back()))) {
        input.remove_suffix(1);
    }
    return input;
}

inline std::vector<std::string_view> split_ascii_whitespace(std::string_view input) {
    std::vector<std::string_view> tokens;
    size_t i = 0;
    while (i < input.size()) {
        while (i < input.size() && std::isspace(static_cast<unsigned char>(input[i])) != 0) {
            ++i;
        }
        if (i >= input.size()) {
            break;
        }
        size_t start = i;
        while (i < input.size() && std::isspace(static_cast<unsigned char>(input[i])) == 0) {
            ++i;
        }
        tokens.emplace_back(input.substr(start, i - start));
    }
    return tokens;
}

}  // namespace Hummingbird::Core::Utils
