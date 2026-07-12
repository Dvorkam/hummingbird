#pragma once

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>
#include <vector>

#include "core/platform_api/IGraphicsContext.h"
#include "core/utils/AssetPath.h"
#include "core/utils/Log.h"
#include "core/utils/StringUtils.h"
#include "style/types/ComputedStyle.h"

namespace Hummingbird::Layout::TextStyleUtils {

enum class FontFamily {
    Sans,
    Monospace,
};

inline std::string normalize_family_name(std::string_view name) {
    std::string_view trimmed = Core::Utils::trim_ascii_whitespace(name);
    std::string lower = Core::Utils::to_lower(trimmed);
    std::string normalized;
    normalized.reserve(lower.size());
    bool in_space = false;
    for (char c : lower) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            if (!in_space) {
                normalized.push_back(' ');
                in_space = true;
            }
        } else {
            normalized.push_back(c);
            in_space = false;
        }
    }
    if (normalized.size() >= 2) {
        char first = normalized.front();
        char last = normalized.back();
        if ((first == '"' && last == '"') || (first == '\'' && last == '\'')) {
            normalized = normalized.substr(1, normalized.size() - 2);
        }
    }
    return normalized;
}

inline std::vector<std::string> split_font_families(std::string_view list) {
    std::vector<std::string> families;
    size_t start = 0;
    while (start <= list.size()) {
        size_t comma = list.find(',', start);
        std::string_view segment =
            comma == std::string_view::npos ? list.substr(start) : list.substr(start, comma - start);
        std::string normalized = normalize_family_name(segment);
        if (!normalized.empty()) {
            families.push_back(std::move(normalized));
        }
        if (comma == std::string_view::npos) {
            break;
        }
        start = comma + 1;
    }
    if (families.size() == 1 && list.find(',') == std::string_view::npos) {
        const std::string& single = families.front();
        auto split_suffix = [&](std::string_view suffix) {
            if (single.size() <= suffix.size()) {
                return;
            }
            if (!single.ends_with(suffix)) {
                return;
            }
            size_t split_pos = single.size() - suffix.size();
            if (split_pos == 0 || single[split_pos - 1] != ' ') {
                return;
            }
            std::string prefix = single.substr(0, split_pos - 1);
            if (prefix.empty()) {
                return;
            }
            families.clear();
            families.push_back(std::move(prefix));
            families.emplace_back(suffix);
        };
        split_suffix("monospace");
        if (families.size() == 1) split_suffix("sans-serif");
        if (families.size() == 1) split_suffix("sans serif");
        if (families.size() == 1) split_suffix("serif");
    }
    return families;
}

inline bool is_monospace_family(const std::string& family) {
    return family == "monospace" || family == "roboto mono" || family == "roboto-mono";
}

inline bool is_sans_family(const std::string& family) {
    return family == "sans-serif" || family == "sans serif" || family == "system-ui" || family == "roboto" ||
           family == "noto sans" || family == "noto-sans" || family == "sans" || family == "serif";
}

inline bool has_token(std::string_view text, std::string_view token) {
    if (token.empty()) {
        return false;
    }
    size_t pos = text.find(token);
    while (pos != std::string_view::npos) {
        const bool left_ok = pos == 0 || std::isspace(static_cast<unsigned char>(text[pos - 1]));
        const size_t end = pos + token.size();
        const bool right_ok = end == text.size() || std::isspace(static_cast<unsigned char>(text[end]));
        if (left_ok && right_ok) {
            return true;
        }
        pos = text.find(token, pos + 1);
    }
    return false;
}

// Minimal font-family mapping: handle common sans/mono names and fall back to bundled Roboto.
inline FontFamily resolve_font_family(const Css::ComputedStyle* style) {
    bool prefers_monospace = style && style->font_monospace;
    std::string_view raw = style ? style->font_face : std::string_view{};
    auto families = split_font_families(raw);
    for (const auto& family : families) {
        if (is_monospace_family(family)) {
            return FontFamily::Monospace;
        }
        if (is_sans_family(family)) {
            return FontFamily::Sans;
        }
    }
    // Some shorthand/font-family parse paths can collapse fallback lists into a
    // whitespace-only token stream (e.g. "roboto mono monospace").
    if (families.size() == 1) {
        const auto& family = families.front();
        if (has_token(family, "monospace") || has_token(family, "roboto mono")) {
            return FontFamily::Monospace;
        }
        if (has_token(family, "sans") || has_token(family, "sans-serif") || has_token(family, "system-ui") ||
            has_token(family, "roboto")) {
            return FontFamily::Sans;
        }
    }
    if (!families.empty()) {
        HB_LOG_WARN("[style] Unsupported font family list '" << raw << "', falling back to Roboto");
    }
    return prefers_monospace ? FontFamily::Monospace : FontFamily::Sans;
}

inline std::string resolve_text_font_path(const Css::ComputedStyle* style) {
    bool bold = style && style->weight == Css::ComputedStyle::FontWeight::Bold;
    bool italic = style && style->style == Css::ComputedStyle::FontStyle::Italic;
    FontFamily family = resolve_font_family(style);

    const char* font_path = nullptr;
    if (family == FontFamily::Monospace) {
        font_path = "assets/fonts/RobotoMono-Regular.ttf";
        if (bold && italic) {
            font_path = "assets/fonts/RobotoMono-BoldItalic.ttf";
        } else if (bold) {
            font_path = "assets/fonts/RobotoMono-Bold.ttf";
        } else if (italic) {
            font_path = "assets/fonts/RobotoMono-Italic.ttf";
        }
    } else {
        font_path = "assets/fonts/Roboto-Regular.ttf";
        if (bold && italic) {
            font_path = "assets/fonts/Roboto-BoldItalic.ttf";
        } else if (bold) {
            font_path = "assets/fonts/Roboto-Bold.ttf";
        } else if (italic) {
            font_path = "assets/fonts/Roboto-Italic.ttf";
        }
    }
    return Hummingbird::Core::Utils::resolve_asset_path_string(font_path);
}

inline TextStyle build_text_style(const Css::ComputedStyle* style) {
    TextStyle text_style;
    FontFamily family = resolve_font_family(style);
    text_style.font_path = resolve_text_font_path(style);
    text_style.font_size = style ? style->font_size : 16.0f;
    text_style.bold = false;
    text_style.italic = false;
    text_style.monospace = family == FontFamily::Monospace;
    text_style.color = style ? style->color : Color{0, 0, 0, 255};
    return text_style;
}

}  // namespace Hummingbird::Layout::TextStyleUtils
