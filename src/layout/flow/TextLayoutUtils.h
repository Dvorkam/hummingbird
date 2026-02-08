#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "style/compute/ComputedStyle.h"

namespace Hummingbird::Layout::TextLayoutUtils {

// Collapse runs of whitespace to a single space; convert newlines/tabs to spaces.
inline std::string collapse_whitespace(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    bool in_space = false;
    for (char c : text) {
        bool is_space = c == ' ' || c == '\n' || c == '\r' || c == '\t';
        if (is_space) {
            if (!in_space) {
                out.push_back(' ');
            }
            in_space = true;
        } else {
            out.push_back(c);
            in_space = false;
        }
    }
    return out;
}

inline std::vector<std::string> tokenize_text(std::string_view text) {
    std::vector<std::string> tokens;
    std::string current;
    for (char c : text) {
        if (c == ' ') {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
            tokens.emplace_back(" ");
        } else {
            current.push_back(c);
        }
    }
    if (!current.empty()) {
        tokens.push_back(current);
    }
    if (tokens.empty()) {
        tokens.emplace_back(" ");
    }
    return tokens;
}

inline std::string build_rendered_text(std::string_view text, const Css::ComputedStyle* style) {
    if (style && style->whitespace == Css::ComputedStyle::WhiteSpace::Preserve) {
        return std::string(text);
    }
    return collapse_whitespace(text);
}

}  // namespace Hummingbird::Layout::TextLayoutUtils
