#include "style/StyleEngine.h"

#include <stddef.h>

#include <cctype>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/ArenaAllocator.h"
#include "core/dom/Element.h"
#include "core/dom/Node.h"
#include "core/platform_api/IGraphicsContext.h"
#include "core/utils/ColorUtils.h"
#include "core/utils/ParseUtils.h"
#include "core/utils/StringUtils.h"
#include "style/ComputedStyle.h"
#include "style/CssValueNames.h"
#include "style/SelectorMatcher.h"
#include "style/StyleDefaults.h"
#include "style/Stylesheet.h"

namespace Hummingbird::Css {

namespace {

float value_to_length(const Value& value, float fallback, float font_size) {
    if (value.type != Value::Type::Length) return fallback;
    if (value.length.unit == Unit::Px) return value.length.value;
    if (value.length.unit == Unit::Em) return value.length.value * font_size;
    return fallback;
}

std::optional<float> parse_length_token(std::string_view token, float font_size) {
    token = Core::Utils::trim_ascii_whitespace(token);
    if (token.empty()) return std::nullopt;
    if (token.size() > 2 && token.ends_with("px")) {
        auto value = Core::Utils::parse_float(token.substr(0, token.size() - 2));
        if (value) return *value;
        return std::nullopt;
    }
    if (token.size() > 2 && token.ends_with("em")) {
        auto value = Core::Utils::parse_float(token.substr(0, token.size() - 2));
        if (value) return *value * font_size;
        return std::nullopt;
    }
    return std::nullopt;
}

std::vector<std::string_view> split_tokens(std::string_view text) {
    std::vector<std::string_view> tokens;
    size_t i = 0;
    while (i < text.size()) {
        while (i < text.size() && std::isspace(static_cast<unsigned char>(text[i]))) {
            ++i;
        }
        if (i >= text.size()) break;
        size_t start = i;
        while (i < text.size() && !std::isspace(static_cast<unsigned char>(text[i]))) {
            ++i;
        }
        tokens.push_back(text.substr(start, i - start));
    }
    return tokens;
}

void apply_edge(EdgeSizes& edges, float value) {
    edges.top = edges.right = edges.bottom = edges.left = value;
}

struct MatchedProperty {
    int specificity;
    size_t order;
    Value value;
};

struct StyleResult {
    ComputedStyle style;
    StyleDefaults::StyleOverrides overrides;
};

struct PropertyHash {
    size_t operator()(Property property) const { return static_cast<size_t>(property); }
};

using PropertyMap = std::unordered_map<Property, MatchedProperty, PropertyHash>;
using CustomPropertyMap = std::unordered_map<std::string, MatchedProperty>;

struct MatchedDeclarations {
    PropertyMap properties;
    CustomPropertyMap custom_properties;
};

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

std::optional<Color> resolve_var_color(const ComputedStyle& style, const ComputedStyle* parent_style,
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
        return resolve_var_color(style, parent_style, candidate);
    }
    return Core::Utils::parse_html_color(candidate);
}

MatchedDeclarations collect_matched_properties(const Stylesheet& sheet, const DOM::Node* node) {
    MatchedDeclarations matched;
    size_t order = 0;

    const auto* element = dynamic_cast<const DOM::Element*>(node);
    if (!element) return matched;

    for (const auto& rule : sheet.rules) {
        for (const auto& selector : rule.selectors) {
            if (!matches_selector(node, selector)) continue;
            int spec = selector.specificity();
            for (const auto& decl : rule.declarations) {
                if (decl.property == Property::Custom) {
                    if (decl.custom_property.empty()) {
                        ++order;
                        continue;
                    }
                    auto it = matched.custom_properties.find(decl.custom_property);
                    if (it == matched.custom_properties.end() || spec > it->second.specificity ||
                        (spec == it->second.specificity && order > it->second.order)) {
                        matched.custom_properties[decl.custom_property] = {spec, order, decl.value};
                    }
                    ++order;
                    continue;
                }
                auto it = matched.properties.find(decl.property);
                if (it == matched.properties.end() || spec > it->second.specificity ||
                    (spec == it->second.specificity && order > it->second.order)) {
                    matched.properties[decl.property] = {spec, order, decl.value};
                }
                ++order;
            }
        }
    }

    return matched;
}

void apply_length_if_present(const PropertyMap& properties, Property property, float& target, float font_size) {
    auto it = properties.find(property);
    if (it != properties.end()) {
        target = value_to_length(it->second.value, target, font_size);
    }
}

void apply_optional_length_if_present(const PropertyMap& properties, Property property, std::optional<float>& target,
                                      float font_size) {
    auto it = properties.find(property);
    if (it == properties.end()) {
        return;
    }
    const auto& value = it->second.value;
    if (value.type == Value::Type::Length) {
        if (value.length.unit == Unit::Px) {
            target = value.length.value;
        } else if (value.length.unit == Unit::Em) {
            target = value.length.value * font_size;
        }
    } else if (value.type == Value::Type::Number) {
        target = value.number;
    }
}

void apply_margin_if_present(const PropertyMap& properties, Property property, float& target, bool& auto_flag,
                             float font_size) {
    auto it = properties.find(property);
    if (it == properties.end()) {
        return;
    }
    const auto& value = it->second.value;
    if (value.type == Value::Type::Identifier && value.ident == ValueNames::Auto) {
        auto_flag = true;
        target = 0.0f;
        return;
    }
    auto_flag = false;
    target = value_to_length(value, target, font_size);
}

void apply_border_style(ComputedStyle& style, const Value& value) {
    if (value.type != Value::Type::Identifier) return;
    if (value.ident == ValueNames::Solid) {
        style.border_style = ComputedStyle::BorderStyle::Solid;
    } else if (value.ident == ValueNames::Outset) {
        style.border_style = ComputedStyle::BorderStyle::Outset;
    } else if (value.ident == ValueNames::Inset) {
        style.border_style = ComputedStyle::BorderStyle::Inset;
    } else if (value.ident == ValueNames::Ridge) {
        style.border_style = ComputedStyle::BorderStyle::Ridge;
    } else if (value.ident == ValueNames::Groove) {
        style.border_style = ComputedStyle::BorderStyle::Groove;
    }
}

void apply_position_property(const PropertyMap& properties, ComputedStyle& style) {
    auto it = properties.find(Property::Position);
    if (it == properties.end() || it->second.value.type != Value::Type::Identifier) {
        return;
    }
    const auto& ident = it->second.value.ident;
    if (ident == ValueNames::Relative) {
        style.position = ComputedStyle::Position::Relative;
    } else if (ident == ValueNames::Absolute) {
        style.position = ComputedStyle::Position::Absolute;
    } else if (ident == ValueNames::Static) {
        style.position = ComputedStyle::Position::Static;
    }
}

void apply_z_index_property(const PropertyMap& properties, ComputedStyle& style) {
    auto it = properties.find(Property::ZIndex);
    if (it == properties.end()) {
        return;
    }
    const auto& value = it->second.value;
    if (value.type == Value::Type::Number) {
        style.z_index = static_cast<int>(value.number);
    } else if (value.type == Value::Type::Identifier && value.ident == ValueNames::Auto) {
        style.z_index.reset();
    }
}

bool apply_display_property(const PropertyMap& properties, ComputedStyle& style) {
    auto display_it = properties.find(Property::Display);
    if (display_it == properties.end() || display_it->second.value.type != Value::Type::Identifier) {
        return false;
    }

    const auto& ident = display_it->second.value.ident;
    if (ident == ValueNames::None) {
        style.display = ComputedStyle::Display::None;
    } else if (ident == ValueNames::Inline) {
        style.display = ComputedStyle::Display::Inline;
    } else if (ident == ValueNames::InlineBlock) {
        style.display = ComputedStyle::Display::InlineBlock;
    } else if (ident == ValueNames::ListItem) {
        style.display = ComputedStyle::Display::ListItem;
    } else if (ident == ValueNames::Block) {
        style.display = ComputedStyle::Display::Block;
    }
    return true;
}

void apply_color_property(const PropertyMap& properties, ComputedStyle& style, StyleDefaults::StyleOverrides& overrides,
                          const ComputedStyle* parent_style) {
    auto color_it = properties.find(Property::Color);
    if (color_it != properties.end() && color_it->second.value.type == Value::Type::Color) {
        style.color = color_it->second.value.color;
        overrides.color = true;
    } else if (color_it != properties.end() && color_it->second.value.type == Value::Type::Identifier) {
        if (auto resolved = resolve_var_color(style, parent_style, color_it->second.value.ident)) {
            style.color = *resolved;
            overrides.color = true;
        }
    }
}

void apply_background_property(const PropertyMap& properties, ComputedStyle& style,
                               StyleDefaults::StyleOverrides& overrides, const ComputedStyle* parent_style) {
    auto bg_it = properties.find(Property::BackgroundColor);
    if (bg_it != properties.end() && bg_it->second.value.type == Value::Type::Color) {
        style.background = bg_it->second.value.color;
        overrides.background = true;
    } else if (bg_it != properties.end() && bg_it->second.value.type == Value::Type::Identifier) {
        if (auto resolved = resolve_var_color(style, parent_style, bg_it->second.value.ident)) {
            style.background = *resolved;
            overrides.background = true;
        }
    }
}

void apply_background_image_property(const PropertyMap& properties, ComputedStyle& style) {
    auto it = properties.find(Property::BackgroundImage);
    if (it == properties.end()) {
        return;
    }
    const auto& value = it->second.value;
    if (value.type == Value::Type::Url) {
        style.background_image = value.ident;
        return;
    }
    if (value.type == Value::Type::Identifier && value.ident == ValueNames::None) {
        style.background_image.reset();
    }
}

void apply_background_repeat_property(const PropertyMap& properties, ComputedStyle& style) {
    auto it = properties.find(Property::BackgroundRepeat);
    if (it == properties.end() || it->second.value.type != Value::Type::Identifier) {
        return;
    }
    auto tokens = split_tokens(it->second.value.ident);
    if (tokens.empty()) {
        return;
    }
    if (tokens.size() >= 2) {
        const auto& x = tokens[0];
        const auto& y = tokens[1];
        if (x == ValueNames::Repeat && y == ValueNames::NoRepeat) {
            style.background_repeat = ComputedStyle::BackgroundRepeat::RepeatX;
            return;
        }
        if (x == ValueNames::NoRepeat && y == ValueNames::Repeat) {
            style.background_repeat = ComputedStyle::BackgroundRepeat::RepeatY;
            return;
        }
        if (x == ValueNames::NoRepeat && y == ValueNames::NoRepeat) {
            style.background_repeat = ComputedStyle::BackgroundRepeat::NoRepeat;
            return;
        }
        if (x == ValueNames::Repeat && y == ValueNames::Repeat) {
            style.background_repeat = ComputedStyle::BackgroundRepeat::Repeat;
            return;
        }
    }
    const auto& ident = tokens[0];
    if (ident == ValueNames::NoRepeat) {
        style.background_repeat = ComputedStyle::BackgroundRepeat::NoRepeat;
    } else if (ident == ValueNames::RepeatX) {
        style.background_repeat = ComputedStyle::BackgroundRepeat::RepeatX;
    } else if (ident == ValueNames::RepeatY) {
        style.background_repeat = ComputedStyle::BackgroundRepeat::RepeatY;
    } else if (ident == ValueNames::Repeat) {
        style.background_repeat = ComputedStyle::BackgroundRepeat::Repeat;
    }
}

void apply_background_position_property(const PropertyMap& properties, ComputedStyle& style) {
    auto it = properties.find(Property::BackgroundPosition);
    if (it == properties.end() || it->second.value.type != Value::Type::Identifier) {
        return;
    }
    auto tokens = split_tokens(it->second.value.ident);
    if (tokens.empty()) {
        return;
    }

    bool horizontal_set = false;
    bool vertical_set = false;

    auto apply_keyword = [&](std::string_view token) {
        if (token == ValueNames::Left) {
            style.background_position.horizontal = ComputedStyle::BackgroundPosition::Horizontal::Left;
            horizontal_set = true;
            return true;
        }
        if (token == ValueNames::Right) {
            style.background_position.horizontal = ComputedStyle::BackgroundPosition::Horizontal::Right;
            horizontal_set = true;
            return true;
        }
        if (token == ValueNames::Top) {
            style.background_position.vertical = ComputedStyle::BackgroundPosition::Vertical::Top;
            vertical_set = true;
            return true;
        }
        if (token == ValueNames::Bottom) {
            style.background_position.vertical = ComputedStyle::BackgroundPosition::Vertical::Bottom;
            vertical_set = true;
            return true;
        }
        if (token == ValueNames::Center) {
            if (!horizontal_set) {
                style.background_position.horizontal = ComputedStyle::BackgroundPosition::Horizontal::Center;
                horizontal_set = true;
                return true;
            }
            if (!vertical_set) {
                style.background_position.vertical = ComputedStyle::BackgroundPosition::Vertical::Center;
                vertical_set = true;
                return true;
            }
        }
        return false;
    };

    const float font_size = style.font_size;
    size_t index = 0;
    if (index < tokens.size()) {
        if (auto length = parse_length_token(tokens[index], font_size)) {
            style.background_position.offset_x = *length;
            horizontal_set = true;
        } else {
            apply_keyword(tokens[index]);
        }
        ++index;
    }
    if (index < tokens.size()) {
        if (auto length = parse_length_token(tokens[index], font_size)) {
            style.background_position.offset_y = *length;
            vertical_set = true;
        } else {
            apply_keyword(tokens[index]);
        }
    }

    if (horizontal_set && !vertical_set) {
        style.background_position.vertical = ComputedStyle::BackgroundPosition::Vertical::Center;
    } else if (vertical_set && !horizontal_set) {
        style.background_position.horizontal = ComputedStyle::BackgroundPosition::Horizontal::Center;
    }
}

void apply_background_size_property(const PropertyMap& properties, ComputedStyle& style) {
    auto it = properties.find(Property::BackgroundSize);
    if (it == properties.end() || it->second.value.type != Value::Type::Identifier) {
        return;
    }
    auto tokens = split_tokens(it->second.value.ident);
    if (tokens.empty()) {
        return;
    }

    if (tokens[0] == ValueNames::Cover) {
        style.background_size.type = ComputedStyle::BackgroundSize::Type::Cover;
        style.background_size.width.reset();
        style.background_size.height.reset();
        return;
    }
    if (tokens[0] == ValueNames::Contain) {
        style.background_size.type = ComputedStyle::BackgroundSize::Type::Contain;
        style.background_size.width.reset();
        style.background_size.height.reset();
        return;
    }
    if (tokens[0] == ValueNames::Auto) {
        style.background_size.type = ComputedStyle::BackgroundSize::Type::Auto;
        style.background_size.width.reset();
        style.background_size.height.reset();
        return;
    }

    const float font_size = style.font_size;
    auto width = parse_length_token(tokens[0], font_size);
    if (!width) {
        return;
    }
    style.background_size.type = ComputedStyle::BackgroundSize::Type::Length;
    style.background_size.width = *width;
    style.background_size.height.reset();
    if (tokens.size() > 1) {
        if (auto height = parse_length_token(tokens[1], font_size)) {
            style.background_size.height = *height;
        }
    }
}

void apply_properties_to_style(const PropertyMap& properties, ComputedStyle& style,
                               StyleDefaults::StyleOverrides& overrides, bool& display_set, float parent_font_size,
                               const ComputedStyle* parent_style) {
    display_set = apply_display_property(properties, style);
    apply_position_property(properties, style);

    auto font_size_it = properties.find(Property::FontSize);
    if (font_size_it != properties.end()) {
        const auto& value = font_size_it->second.value;
        if (value.type == Value::Type::Length) {
            if (value.length.unit == Unit::Px) {
                style.font_size = value.length.value;
                overrides.font_size = true;
            } else if (value.length.unit == Unit::Em) {
                style.font_size = value.length.value * parent_font_size;
                overrides.font_size = true;
            }
        }
    }

    auto line_height_it = properties.find(Property::LineHeight);
    if (line_height_it != properties.end()) {
        const auto& value = line_height_it->second.value;
        if (value.type == Value::Type::Length && value.length.unit == Unit::Px) {
            style.line_height = value.length.value;
            overrides.line_height = true;
        } else if (value.type == Value::Type::Length && value.length.unit == Unit::Em) {
            style.line_height = value.length.value * style.font_size;
            overrides.line_height = true;
        } else if (value.type == Value::Type::Number) {
            style.line_height = value.number * style.font_size;
            overrides.line_height = true;
        }
    }

    // margin / padding shorthand and individual edges
    auto margin_it = properties.find(Property::Margin);
    if (margin_it != properties.end()) {
        apply_edge(style.margin, value_to_length(margin_it->second.value, 0.0f, style.font_size));
    }
    auto padding_it = properties.find(Property::Padding);
    if (padding_it != properties.end()) {
        apply_edge(style.padding, value_to_length(padding_it->second.value, 0.0f, style.font_size));
    }

    apply_length_if_present(properties, Property::MarginTop, style.margin.top, style.font_size);
    apply_margin_if_present(properties, Property::MarginRight, style.margin.right, style.margin_right_auto,
                            style.font_size);
    apply_length_if_present(properties, Property::MarginBottom, style.margin.bottom, style.font_size);
    apply_margin_if_present(properties, Property::MarginLeft, style.margin.left, style.margin_left_auto,
                            style.font_size);

    apply_length_if_present(properties, Property::PaddingTop, style.padding.top, style.font_size);
    apply_length_if_present(properties, Property::PaddingRight, style.padding.right, style.font_size);
    apply_length_if_present(properties, Property::PaddingBottom, style.padding.bottom, style.font_size);
    apply_length_if_present(properties, Property::PaddingLeft, style.padding.left, style.font_size);

    auto border_width_it = properties.find(Property::BorderWidth);
    if (border_width_it != properties.end()) {
        apply_edge(style.border_width, value_to_length(border_width_it->second.value, 0.0f, style.font_size));
    }
    auto border_color_it = properties.find(Property::BorderColor);
    if (border_color_it != properties.end() && border_color_it->second.value.type == Value::Type::Color) {
        style.border_color = border_color_it->second.value.color;
    }
    auto border_style_it = properties.find(Property::BorderStyle);
    if (border_style_it != properties.end()) {
        apply_border_style(style, border_style_it->second.value);
    }

    apply_optional_length_if_present(properties, Property::Width, style.width, style.font_size);
    apply_optional_length_if_present(properties, Property::Height, style.height, style.font_size);
    apply_optional_length_if_present(properties, Property::MaxWidth, style.max_width, style.font_size);
    apply_optional_length_if_present(properties, Property::Top, style.top, style.font_size);
    apply_optional_length_if_present(properties, Property::Right, style.right, style.font_size);
    apply_optional_length_if_present(properties, Property::Bottom, style.bottom, style.font_size);
    apply_optional_length_if_present(properties, Property::Left, style.left, style.font_size);
    apply_z_index_property(properties, style);

    auto text_align_it = properties.find(Property::TextAlign);
    if (text_align_it != properties.end() && text_align_it->second.value.type == Value::Type::Identifier) {
        const auto& ident = text_align_it->second.value.ident;
        if (ident == ValueNames::Left) {
            style.text_align = ComputedStyle::TextAlign::Left;
            overrides.text_align = true;
        } else if (ident == ValueNames::Center) {
            style.text_align = ComputedStyle::TextAlign::Center;
            overrides.text_align = true;
        } else if (ident == ValueNames::Right) {
            style.text_align = ComputedStyle::TextAlign::Right;
            overrides.text_align = true;
        }
    }

    auto text_decoration_it = properties.find(Property::TextDecoration);
    if (text_decoration_it != properties.end() && text_decoration_it->second.value.type == Value::Type::Identifier) {
        const auto& ident = text_decoration_it->second.value.ident;
        if (ident == ValueNames::Underline) {
            style.underline = true;
            overrides.underline = true;
        } else if (ident == ValueNames::None) {
            style.underline = false;
            overrides.underline = true;
        }
    }

    auto whitespace_it = properties.find(Property::WhiteSpace);
    if (whitespace_it != properties.end() && whitespace_it->second.value.type == Value::Type::Identifier) {
        const auto& ident = whitespace_it->second.value.ident;
        if (ident == ValueNames::Normal) {
            style.whitespace = ComputedStyle::WhiteSpace::Normal;
            overrides.whitespace = true;
        } else if (ident == ValueNames::NoWrap) {
            style.whitespace = ComputedStyle::WhiteSpace::NoWrap;
            overrides.whitespace = true;
        }
    }

    auto font_family_it = properties.find(Property::FontFamily);
    if (font_family_it != properties.end() && font_family_it->second.value.type == Value::Type::Identifier) {
        style.font_face = font_family_it->second.value.ident;
        overrides.font_face = true;
    }

    auto font_weight_it = properties.find(Property::FontWeight);
    if (font_weight_it != properties.end()) {
        const auto& value = font_weight_it->second.value;
        if (value.type == Value::Type::Identifier) {
            if (value.ident == ValueNames::Bold) {
                style.weight = ComputedStyle::FontWeight::Bold;
                overrides.weight = true;
            } else if (value.ident == ValueNames::Normal) {
                style.weight = ComputedStyle::FontWeight::Normal;
                overrides.weight = true;
            }
        } else if (value.type == Value::Type::Number) {
            style.weight = value.number >= 600.0f ? ComputedStyle::FontWeight::Bold : ComputedStyle::FontWeight::Normal;
            overrides.weight = true;
        }
    }

    auto font_style_it = properties.find(Property::FontStyle);
    if (font_style_it != properties.end() && font_style_it->second.value.type == Value::Type::Identifier) {
        const auto& ident = font_style_it->second.value.ident;
        if (ident == ValueNames::Italic) {
            style.style = ComputedStyle::FontStyle::Italic;
            overrides.style = true;
        } else if (ident == ValueNames::Normal) {
            style.style = ComputedStyle::FontStyle::Normal;
            overrides.style = true;
        }
    }

    auto float_it = properties.find(Property::Float);
    if (float_it != properties.end() && float_it->second.value.type == Value::Type::Identifier) {
        const auto& ident = float_it->second.value.ident;
        if (ident == ValueNames::Left) {
            style.float_type = ComputedStyle::Float::Left;
        } else if (ident == ValueNames::Right) {
            style.float_type = ComputedStyle::Float::Right;
        } else if (ident == ValueNames::None) {
            style.float_type = ComputedStyle::Float::None;
        }
    }

    apply_color_property(properties, style, overrides, parent_style);
    apply_background_property(properties, style, overrides, parent_style);
    apply_background_image_property(properties, style);
    apply_background_repeat_property(properties, style);
    apply_background_position_property(properties, style);
    apply_background_size_property(properties, style);
}

void apply_custom_properties(const CustomPropertyMap& properties, ComputedStyle& style) {
    for (const auto& [name, property] : properties) {
        style.custom_properties[name] = property.value.ident;
    }
}

void apply_non_inheritable(ComputedStyle& target, const ComputedStyle& source) {
    target.margin = source.margin;
    target.margin_left_auto = source.margin_left_auto;
    target.margin_right_auto = source.margin_right_auto;
    target.padding = source.padding;
    target.width = source.width;
    target.height = source.height;
    target.max_width = source.max_width;
    target.display = source.display;
    target.border_width = source.border_width;
    target.border_color = source.border_color;
    target.border_style = source.border_style;
    target.background = source.background;
    target.background_image = source.background_image;
    target.background_repeat = source.background_repeat;
    target.background_position = source.background_position;
    target.background_size = source.background_size;
    target.position = source.position;
    target.top = source.top;
    target.right = source.right;
    target.bottom = source.bottom;
    target.left = source.left;
    target.z_index = source.z_index;
    target.float_type = source.float_type;
}

void apply_inheritable_overrides(ComputedStyle& target, const ComputedStyle& source,
                                 const StyleDefaults::StyleOverrides& overrides) {
    if (overrides.color) target.color = source.color;
    if (overrides.underline) target.underline = source.underline;
    if (overrides.link_color) target.link_color = source.link_color;
    if (overrides.vlink_color) target.vlink_color = source.vlink_color;
    if (overrides.whitespace) target.whitespace = source.whitespace;
    if (overrides.font_monospace) target.font_monospace = source.font_monospace;
    if (overrides.weight) target.weight = source.weight;
    if (overrides.style) target.style = source.style;
    if (overrides.font_size) target.font_size = source.font_size;
    if (overrides.font_face) target.font_face = source.font_face;
    if (overrides.text_align) target.text_align = source.text_align;
    if (overrides.background) target.background = source.background;
    if (overrides.line_height) target.line_height = source.line_height;
}

// Returns a computed style based on matching rules and parent style (for inheritance in the future).
StyleResult build_style_for(const Stylesheet& sheet, const DOM::Node* node, const ComputedStyle* parent_style) {
    StyleResult result{default_computed_style(), {}};
    ComputedStyle& style = result.style;
    MatchedDeclarations matched = collect_matched_properties(sheet, node);
    bool display_set = matched.properties.find(Property::Display) != matched.properties.end();

    // Minimal UA defaults for basic HTML readability.
    if (const auto* element = dynamic_cast<const DOM::Element*>(node)) {
        StyleDefaults::apply_user_agent_defaults(*element, style, result.overrides, display_set, parent_style);
        StyleDefaults::apply_legacy_attributes(*element, style, result.overrides);
    }

    apply_custom_properties(matched.custom_properties, style);

    float parent_font_size = parent_style ? parent_style->font_size : style.font_size;
    apply_properties_to_style(matched.properties, style, result.overrides, display_set, parent_font_size, parent_style);

    return result;
}

}  // namespace

void StyleEngine::compute_node(const Stylesheet& sheet, DOM::Node* node, const ComputedStyle* parent_style) {
    ComputedStyle base = parent_style ? *parent_style : default_computed_style();
    StyleResult own = build_style_for(sheet, node, parent_style);

    // Non-inheritable box properties come from the computed (own) style.
    apply_non_inheritable(base, own.style);

    // Inheritable text properties: only elements introduce overrides; text nodes inherit.
    if (dynamic_cast<DOM::Element*>(node)) {
        apply_inheritable_overrides(base, own.style, own.overrides);
    }

    for (const auto& [name, value] : own.style.custom_properties) {
        base.custom_properties[name] = value;
    }

    ComputedStyle style = base;

    node->set_computed_style(std::make_shared<ComputedStyle>(style));

    for (const auto& child : node->get_children()) {
        compute_node(sheet, child.get(), node->get_computed_style().get());
    }
}

void StyleEngine::apply(const Stylesheet& sheet, DOM::Node* root) {
    if (!root) return;
    compute_node(sheet, root, nullptr);
}

}  // namespace Hummingbird::Css
