#pragma once

#include <algorithm>
#include <string_view>

namespace Hummingbird::Core::Utils {

inline std::string_view::size_type clamp_caret(std::string_view::size_type caret, std::string_view text) {
    return std::min(caret, text.size());
}

inline std::string_view::size_type prev_codepoint(std::string_view text, std::string_view::size_type caret) {
    caret = clamp_caret(caret, text);
    if (caret == 0) return 0;
    std::string_view::size_type i = caret - 1;
    while (i > 0 && (static_cast<unsigned char>(text[i]) & 0xC0) == 0x80) {
        --i;
    }
    return i;
}

inline std::string_view::size_type next_codepoint(std::string_view text, std::string_view::size_type caret) {
    caret = clamp_caret(caret, text);
    if (caret >= text.size()) return text.size();
    std::string_view::size_type i = caret + 1;
    while (i < text.size() && (static_cast<unsigned char>(text[i]) & 0xC0) == 0x80) {
        ++i;
    }
    return i;
}

}  // namespace Hummingbird::Core::Utils
