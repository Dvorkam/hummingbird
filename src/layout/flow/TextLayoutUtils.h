#pragma once

#include <cctype>
#include <string>
#include <string_view>
#include <vector>

#include "style/types/ComputedStyle.h"

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
    std::string rendered;
    if (style && style->whitespace == Css::ComputedStyle::WhiteSpace::Preserve) {
        rendered = std::string(text);
    } else {
        rendered = collapse_whitespace(text);
    }

    if (!style) {
        return rendered;
    }

    auto to_lower_char = [](char c) -> char { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); };
    auto to_upper_char = [](char c) -> char { return static_cast<char>(std::toupper(static_cast<unsigned char>(c))); };

    switch (style->text_transform) {
        case Css::ComputedStyle::TextTransform::Uppercase:
            for (char& c : rendered) {
                c = to_upper_char(c);
            }
            break;
        case Css::ComputedStyle::TextTransform::Lowercase:
            for (char& c : rendered) {
                c = to_lower_char(c);
            }
            break;
        case Css::ComputedStyle::TextTransform::Capitalize: {
            bool start_of_word = true;
            for (char& c : rendered) {
                if (std::isalnum(static_cast<unsigned char>(c))) {
                    c = start_of_word ? to_upper_char(c) : to_lower_char(c);
                    start_of_word = false;
                } else {
                    start_of_word = true;
                }
            }
            break;
        }
        case Css::ComputedStyle::TextTransform::None:
            break;
    }

    return rendered;
}

}  // namespace Hummingbird::Layout::TextLayoutUtils
