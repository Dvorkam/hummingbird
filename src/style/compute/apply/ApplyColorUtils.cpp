#include "style/compute/apply/ApplyColorUtils.h"

#include <optional>
#include <string>
#include <string_view>

#include "core/utils/ColorUtils.h"
#include "core/utils/StringUtils.h"

namespace Hummingbird::Css::Apply {

namespace {
struct VarExpression {
    std::string_view name;
    std::string_view fallback;
    bool has_fallback = false;
};

bool is_var_function(std::string_view value) {
    auto trimmed = Core::Utils::trim_ascii_whitespace(value);
    return trimmed.size() >= 5 && trimmed.starts_with("var(") && trimmed.ends_with(")");
}

std::optional<VarExpression> parse_var_expression(std::string_view value) {
    auto trimmed = Core::Utils::trim_ascii_whitespace(value);
    if (!is_var_function(trimmed)) {
        return std::nullopt;
    }
    trimmed.remove_prefix(4);
    trimmed.remove_suffix(1);
    if (trimmed.empty()) {
        return std::nullopt;
    }
    size_t comma = trimmed.find(',');
    if (comma == std::string_view::npos) {
        auto name = Core::Utils::trim_ascii_whitespace(trimmed);
        if (name.empty()) {
            return std::nullopt;
        }
        return VarExpression{name, {}, false};
    }
    auto name = Core::Utils::trim_ascii_whitespace(trimmed.substr(0, comma));
    auto fallback = Core::Utils::trim_ascii_whitespace(trimmed.substr(comma + 1));
    if (name.empty()) {
        return std::nullopt;
    }
    return VarExpression{name, fallback, !fallback.empty()};
}

std::optional<std::string_view> resolve_custom_property(const ComputedStyle& style, const ComputedStyle* parent_style,
                                                        std::string_view name) {
    if (name.empty()) {
        return std::nullopt;
    }
    std::string key(name);
    auto it = style.custom_properties.find(key);
    if (it != style.custom_properties.end()) {
        return it->second;
    }
    if (parent_style) {
        auto parent_it = parent_style->custom_properties.find(key);
        if (parent_it != parent_style->custom_properties.end()) {
            return parent_it->second;
        }
    }
    return std::nullopt;
}

}  // namespace

std::optional<std::string> resolve_var_text(const ComputedStyle& style, const ComputedStyle* parent_style,
                                            std::string_view value) {
    auto parsed = parse_var_expression(value);
    if (!parsed) {
        return std::nullopt;
    }
    auto resolved = resolve_custom_property(style, parent_style, parsed->name);
    std::string_view candidate = resolved.has_value() ? *resolved : parsed->fallback;
    if (candidate.empty()) {
        return std::nullopt;
    }
    if (is_var_function(candidate)) {
        return resolve_var_text(style, parent_style, candidate);
    }
    return std::string(candidate);
}

std::optional<Color> resolve_var_color(const ComputedStyle& style, const ComputedStyle* parent_style,
                                       std::string_view value) {
    auto text = resolve_var_text(style, parent_style, value);
    if (!text) {
        return std::nullopt;
    }
    return Core::Utils::parse_html_color(*text);
}

}  // namespace Hummingbird::Css::Apply
