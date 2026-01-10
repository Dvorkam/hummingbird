#include "style/StyleDefaults.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>

#include "core/dom/Element.h"
#include "html/HtmlAttributeNames.h"
#include "html/HtmlTagNames.h"

namespace Hummingbird::Css::StyleDefaults {

void apply_user_agent_defaults(const DOM::Element& element, ComputedStyle& style, StyleOverrides& overrides,
                               bool display_set) {
    const auto& tag = element.get_tag_name();
    const auto set_heading = [&](float scale, float margin_em) {
        style.font_size = 16.0f * scale;
        style.weight = ComputedStyle::FontWeight::Bold;
        float m = style.font_size * margin_em;
        style.margin.top = style.margin.bottom = m;
    };

    if (!display_set) {
        if (tag == Hummingbird::Html::TagNames::A || tag == Hummingbird::Html::TagNames::Span ||
            tag == Hummingbird::Html::TagNames::Strong || tag == Hummingbird::Html::TagNames::Em ||
            tag == Hummingbird::Html::TagNames::B || tag == Hummingbird::Html::TagNames::I ||
            tag == Hummingbird::Html::TagNames::Code || tag == Hummingbird::Html::TagNames::Img ||
            tag == Hummingbird::Html::TagNames::Font) {
            style.display = ComputedStyle::Display::Inline;
        } else if (tag == Hummingbird::Html::TagNames::Li) {
            style.display = ComputedStyle::Display::ListItem;
        }
    }

    if (tag == Hummingbird::Html::TagNames::Ul || tag == Hummingbird::Html::TagNames::Ol) {
        style.padding.left = 20.0f;
    } else if (tag == Hummingbird::Html::TagNames::Pre) {
        if (style.whitespace == ComputedStyle::WhiteSpace::Normal) {
            style.whitespace = ComputedStyle::WhiteSpace::Preserve;
        }
        style.font_monospace = true;
        overrides.whitespace = true;
        overrides.font_monospace = true;
    } else if (tag == Hummingbird::Html::TagNames::A) {
        style.color = {0, 0, 255, 255};
        style.underline = true;
        overrides.color = true;
        overrides.underline = true;
    } else if (tag == Hummingbird::Html::TagNames::Code) {
        style.font_monospace = true;
        style.background = Color{230, 230, 230, 255};
        style.padding.left = style.padding.right = 2.0f;
        style.padding.top = style.padding.bottom = 1.0f;
        overrides.font_monospace = true;
        overrides.background = true;
    } else if (tag == Hummingbird::Html::TagNames::Blockquote) {
        style.margin.left = 40.0f;
        style.margin.right = 40.0f;
        style.margin.top = 8.0f;
        style.margin.bottom = 8.0f;
    } else if (tag == Hummingbird::Html::TagNames::Hr) {
        style.height = 2.0f;
        style.margin.top = style.margin.bottom = 8.0f;
        style.background = Color{50, 50, 50, 255};
    } else if (tag == Hummingbird::Html::TagNames::Strong) {
        style.weight = ComputedStyle::FontWeight::Bold;
        overrides.weight = true;
    } else if (tag == Hummingbird::Html::TagNames::Em) {
        style.style = ComputedStyle::FontStyle::Italic;
        overrides.style = true;
    } else if (tag == Hummingbird::Html::TagNames::H1) {
        set_heading(2.0f, 0.67f);
        overrides.font_size = true;
        overrides.weight = true;
    } else if (tag == Hummingbird::Html::TagNames::H2) {
        set_heading(1.5f, 0.83f);
        overrides.font_size = true;
        overrides.weight = true;
    } else if (tag == Hummingbird::Html::TagNames::H3) {
        set_heading(1.17f, 1.0f);
        overrides.font_size = true;
        overrides.weight = true;
    } else if (tag == Hummingbird::Html::TagNames::H4) {
        set_heading(1.0f, 1.33f);
        overrides.font_size = true;
        overrides.weight = true;
    } else if (tag == Hummingbird::Html::TagNames::H5) {
        set_heading(0.83f, 1.67f);
        overrides.font_size = true;
        overrides.weight = true;
    } else if (tag == Hummingbird::Html::TagNames::H6) {
        set_heading(0.67f, 2.33f);
        overrides.font_size = true;
        overrides.weight = true;
    }
}

void apply_legacy_attributes(const DOM::Element& element, ComputedStyle& style, StyleOverrides& overrides) {
    auto matches_name = [](std::string_view a, std::string_view b) {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); ++i) {
            if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i]))) {
                return false;
            }
        }
        return true;
    };

    auto parse_length_value = [](std::string_view value) -> std::optional<float> {
        std::string_view trimmed = value;
        while (!trimmed.empty() && std::isspace(static_cast<unsigned char>(trimmed.front()))) {
            trimmed.remove_prefix(1);
        }
        while (!trimmed.empty() && std::isspace(static_cast<unsigned char>(trimmed.back()))) {
            trimmed.remove_suffix(1);
        }
        if (trimmed.empty()) {
            return std::nullopt;
        }
        std::string temp(trimmed);
        char* end = nullptr;
        float parsed = std::strtof(temp.c_str(), &end);
        if (end == temp.c_str()) {
            return std::nullopt;
        }
        while (end && *end != '\0' && std::isspace(static_cast<unsigned char>(*end))) {
            ++end;
        }
        if (end && *end != '\0') {
            return std::nullopt;
        }
        if (parsed < 0.0f) {
            parsed = 0.0f;
        }
        return parsed;
    };

    auto parse_font_size_value = [](std::string_view value) -> std::optional<float> {
        std::string_view trimmed = value;
        while (!trimmed.empty() && std::isspace(static_cast<unsigned char>(trimmed.front()))) {
            trimmed.remove_prefix(1);
        }
        while (!trimmed.empty() && std::isspace(static_cast<unsigned char>(trimmed.back()))) {
            trimmed.remove_suffix(1);
        }
        if (trimmed.empty()) {
            return std::nullopt;
        }
        std::string temp(trimmed);
        char* end = nullptr;
        long parsed = std::strtol(temp.c_str(), &end, 10);
        if (end == temp.c_str()) {
            return std::nullopt;
        }
        while (end && *end != '\0' && std::isspace(static_cast<unsigned char>(*end))) {
            ++end;
        }
        if (end && *end != '\0') {
            return std::nullopt;
        }
        if (parsed < 1) {
            parsed = 1;
        }
        if (parsed > 7) {
            parsed = 7;
        }
        static constexpr float kFontSizes[] = {10.0f, 13.0f, 16.0f, 18.0f, 24.0f, 32.0f, 48.0f};
        return kFontSizes[parsed - 1];
    };

    auto parse_font_face_value = [](std::string_view value) -> std::string {
        std::string_view trimmed = value;
        while (!trimmed.empty() && std::isspace(static_cast<unsigned char>(trimmed.front()))) {
            trimmed.remove_prefix(1);
        }
        while (!trimmed.empty() && std::isspace(static_cast<unsigned char>(trimmed.back()))) {
            trimmed.remove_suffix(1);
        }
        if (auto comma = trimmed.find(','); comma != std::string_view::npos) {
            trimmed = trimmed.substr(0, comma);
            while (!trimmed.empty() && std::isspace(static_cast<unsigned char>(trimmed.back()))) {
                trimmed.remove_suffix(1);
            }
        }
        if (trimmed.size() >= 2 && ((trimmed.front() == '"' && trimmed.back() == '"') ||
                                    (trimmed.front() == '\'' && trimmed.back() == '\''))) {
            trimmed = trimmed.substr(1, trimmed.size() - 2);
        }
        std::string normalized(trimmed);
        std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return normalized;
    };

    for (const auto& [key, value] : element.get_attributes()) {
        if (matches_name(key, Hummingbird::Html::AttributeNames::Align)) {
            std::string normalized = value;
            std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (normalized == "left") {
                style.text_align = ComputedStyle::TextAlign::Left;
                overrides.text_align = true;
            } else if (normalized == "center") {
                style.text_align = ComputedStyle::TextAlign::Center;
                overrides.text_align = true;
            } else if (normalized == "right") {
                style.text_align = ComputedStyle::TextAlign::Right;
                overrides.text_align = true;
            }
        } else if (matches_name(key, Hummingbird::Html::AttributeNames::NoWrap)) {
            style.whitespace = ComputedStyle::WhiteSpace::NoWrap;
            overrides.whitespace = true;
        } else if (matches_name(key, Hummingbird::Html::AttributeNames::Width) && !style.width.has_value()) {
            if (auto parsed = parse_length_value(value)) {
                style.width = *parsed;
            }
        } else if (matches_name(key, Hummingbird::Html::AttributeNames::Height) && !style.height.has_value()) {
            if (auto parsed = parse_length_value(value)) {
                style.height = *parsed;
            }
        } else if (matches_name(key, Hummingbird::Html::AttributeNames::Size)) {
            if (auto parsed = parse_font_size_value(value)) {
                style.font_size = *parsed;
                overrides.font_size = true;
            }
        } else if (matches_name(key, Hummingbird::Html::AttributeNames::Face)) {
            std::string face = parse_font_face_value(value);
            if (!face.empty()) {
                style.font_face = std::move(face);
                overrides.font_face = true;
            }
        }
    }
}

}  // namespace Hummingbird::Css::StyleDefaults
