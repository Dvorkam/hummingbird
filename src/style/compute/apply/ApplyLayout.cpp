#include "style/compute/apply/ApplyLayout.h"

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/utils/ColorUtils.h"
#include "core/utils/ParseUtils.h"
#include "core/utils/StringUtils.h"
#include "style/compute/StyleValueUtils.h"
#include "style/registry/CssValueNames.h"

namespace Hummingbird::Css::Apply {

namespace {
std::optional<std::pair<float, float>> parse_transform_translate(std::string_view text, float font_size) {
    auto trimmed = Core::Utils::trim_ascii_whitespace(text);
    if (trimmed.empty() || trimmed == ValueNames::None) {
        return std::nullopt;
    }

    auto parse_length_from_tokens = [&](const std::vector<std::string_view>& tokens,
                                        size_t& index) -> std::optional<float> {
        if (index >= tokens.size()) {
            return std::nullopt;
        }
        auto value = StyleValueUtils::parse_length_token(tokens[index], font_size);
        if (value) {
            ++index;
            return *value;
        }
        auto number = Core::Utils::parse_float(tokens[index]);
        if (!number) {
            return std::nullopt;
        }
        float out = *number;
        ++index;
        if (index < tokens.size()) {
            if (tokens[index] == ValueNames::Px) {
                ++index;
            } else if (tokens[index] == ValueNames::Em) {
                out *= font_size;
                ++index;
            }
        }
        return out;
    };

    auto parse_args = [&](std::string_view args) -> std::optional<std::pair<float, float>> {
        args = Core::Utils::trim_ascii_whitespace(args);
        if (args.empty()) {
            return std::nullopt;
        }
        size_t comma = args.find(',');
        std::string_view first = args;
        std::string_view second;
        if (comma != std::string_view::npos) {
            first = args.substr(0, comma);
            second = args.substr(comma + 1);
        }
        auto x = StyleValueUtils::parse_length_or_number(first, font_size);
        if (!x) {
            return std::nullopt;
        }
        float y_value = 0.0f;
        if (!second.empty()) {
            auto y = StyleValueUtils::parse_length_or_number(second, font_size);
            if (!y) {
                return std::nullopt;
            }
            y_value = *y;
        }
        return std::make_pair(*x, y_value);
    };

    if (trimmed.starts_with("translate(") && trimmed.ends_with(")")) {
        auto args = trimmed.substr(10, trimmed.size() - 11);
        return parse_args(args);
    }
    if (trimmed.starts_with("translateX(") && trimmed.ends_with(")")) {
        auto args = trimmed.substr(11, trimmed.size() - 12);
        auto value = StyleValueUtils::parse_length_or_number(args, font_size);
        if (!value) {
            return std::nullopt;
        }
        return std::make_pair(*value, 0.0f);
    }
    if (trimmed.starts_with("translateY(") && trimmed.ends_with(")")) {
        auto args = trimmed.substr(11, trimmed.size() - 12);
        auto value = StyleValueUtils::parse_length_or_number(args, font_size);
        if (!value) {
            return std::nullopt;
        }
        return std::make_pair(0.0f, *value);
    }

    auto tokens = StyleValueUtils::split_tokens(trimmed);
    if (tokens.empty()) {
        return std::nullopt;
    }
    if (tokens[0] == "translate") {
        size_t index = 1;
        auto x = parse_length_from_tokens(tokens, index);
        if (!x) {
            return std::nullopt;
        }
        float y_value = 0.0f;
        if (index < tokens.size()) {
            if (auto y = parse_length_from_tokens(tokens, index)) {
                y_value = *y;
            }
        }
        return std::make_pair(*x, y_value);
    }
    if (tokens[0] == "translateX") {
        size_t index = 1;
        auto x = parse_length_from_tokens(tokens, index);
        if (!x) {
            return std::nullopt;
        }
        return std::make_pair(*x, 0.0f);
    }
    if (tokens[0] == "translateY") {
        size_t index = 1;
        auto y = parse_length_from_tokens(tokens, index);
        if (!y) {
            return std::nullopt;
        }
        return std::make_pair(0.0f, *y);
    }
    return std::nullopt;
}

void apply_edge(EdgeSizes& edges, float value) {
    edges.top = edges.right = edges.bottom = edges.left = value;
}

// A single corner radius value. Percentages are kept unresolved (they need the
// box dimensions, which only exist at paint time); px/em resolve to px here.
CornerRadius value_to_corner_radius(const Value& value, float font_size) {
    CornerRadius corner;
    if (value.type == Value::Type::Number) {
        corner.value = std::max(0.0f, value.number);
        return corner;
    }
    if (value.type == Value::Type::Length && value.length.unit == Unit::Percent) {
        corner.value = std::max(0.0f, value.length.value);
        corner.percent = true;
        return corner;
    }
    corner.value = std::max(0.0f, StyleValueUtils::value_to_length(value, 0.0f, font_size));
    return corner;
}

void apply_optional_length(std::optional<float>& target, bool& is_percent, const Value& value, float font_size) {
    if (value.type == Value::Type::Length) {
        if (value.length.unit == Unit::Px) {
            target = value.length.value;
            is_percent = false;
        } else if (value.length.unit == Unit::Em) {
            target = value.length.value * font_size;
            is_percent = false;
        } else if (value.length.unit == Unit::Percent) {
            target = value.length.value;
            is_percent = true;
        }
    } else if (value.type == Value::Type::Number) {
        target = value.number;
        is_percent = false;
    }
}

void apply_length(float& target, const Value& value, float font_size) {
    target = StyleValueUtils::value_to_length(value, target, font_size);
}

void apply_margin_value(float& target, bool& auto_flag, const Value& value, float font_size) {
    if (value.type == Value::Type::Identifier && value.ident == ValueNames::Auto) {
        auto_flag = true;
        target = 0.0f;
        return;
    }
    auto_flag = false;
    target = StyleValueUtils::value_to_length(value, target, font_size);
}

void apply_border_style(ComputedStyle& style, const Value& value) {
    if (value.type != Value::Type::Identifier) return;
    if (value.ident == ValueNames::None) {
        style.border_style = ComputedStyle::BorderStyle::None;
    } else if (value.ident == ValueNames::Solid) {
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

std::optional<Color> parse_outline_color_token(std::string_view token) {
    token = Core::Utils::trim_ascii_whitespace(token);
    if (token.empty()) {
        return std::nullopt;
    }
    if (token.starts_with("#")) {
        return Core::Utils::parse_hex_color(token.substr(1));
    }
    if (token == ValueNames::Red) return Color{255, 0, 0, 255};
    if (token == ValueNames::Blue) return Color{0, 0, 255, 255};
    if (token == ValueNames::Black) return Color{0, 0, 0, 255};
    if (token == ValueNames::White) return Color{255, 255, 255, 255};
    return std::nullopt;
}

void apply_outline_shorthand(ComputedStyle& style, const Value& value) {
    if (value.type != Value::Type::Identifier) {
        return;
    }
    for (auto token : StyleValueUtils::split_tokens(value.ident)) {
        if (auto length = StyleValueUtils::parse_length_token(token, style.font_size)) {
            style.outline_width = std::max(0.0f, *length);
            continue;
        }
        if (auto color = parse_outline_color_token(token)) {
            style.outline_color = *color;
        }
    }
}

void apply_position_value(ComputedStyle& style, const Value& value) {
    if (value.type != Value::Type::Identifier) {
        return;
    }
    const auto& ident = value.ident;
    if (ident == ValueNames::Relative) {
        style.position = ComputedStyle::Position::Relative;
    } else if (ident == ValueNames::Absolute) {
        style.position = ComputedStyle::Position::Absolute;
    } else if (ident == ValueNames::Static) {
        style.position = ComputedStyle::Position::Static;
    }
}

std::optional<ComputedStyle::Overflow> parse_overflow_value(const Value& value) {
    if (value.type != Value::Type::Identifier) {
        return std::nullopt;
    }
    if (value.ident == ValueNames::Visible) {
        return ComputedStyle::Overflow::Visible;
    }
    if (value.ident == ValueNames::Hidden) {
        return ComputedStyle::Overflow::Hidden;
    }
    if (value.ident == ValueNames::Scroll) {
        return ComputedStyle::Overflow::Scroll;
    }
    if (value.ident == ValueNames::Auto) {
        return ComputedStyle::Overflow::Auto;
    }
    return std::nullopt;
}

void apply_z_index_value(ComputedStyle& style, const Value& value) {
    if (value.type == Value::Type::Number) {
        style.z_index = static_cast<int>(value.number);
    } else if (value.type == Value::Type::Identifier && value.ident == ValueNames::Auto) {
        style.z_index.reset();
    }
}

void apply_opacity_value(ComputedStyle& style, const Value& value) {
    float opacity = style.opacity;
    if (value.type == Value::Type::Number) {
        opacity = value.number;
    } else if (value.type == Value::Type::Length && value.length.unit == Unit::Percent) {
        opacity = value.length.value / 100.0f;
    } else {
        return;
    }
    style.opacity = std::clamp(opacity, 0.0f, 1.0f);
}

bool apply_display_value(ComputedStyle& style, const Value& value) {
    if (value.type != Value::Type::Identifier) {
        return false;
    }

    const auto& ident = value.ident;
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
    } else if (ident == ValueNames::Flex) {
        style.display = ComputedStyle::Display::Flex;
    }
    return true;
}

void apply_flex_direction_value(ComputedStyle& style, const Value& value) {
    if (value.type != Value::Type::Identifier) {
        return;
    }
    if (value.ident == ValueNames::Row) {
        style.flex_direction = ComputedStyle::FlexDirection::Row;
    } else if (value.ident == ValueNames::RowReverse) {
        style.flex_direction = ComputedStyle::FlexDirection::RowReverse;
    } else if (value.ident == ValueNames::Column) {
        style.flex_direction = ComputedStyle::FlexDirection::Column;
    } else if (value.ident == ValueNames::ColumnReverse) {
        style.flex_direction = ComputedStyle::FlexDirection::ColumnReverse;
    }
}

void apply_flex_wrap_value(ComputedStyle& style, const Value& value) {
    if (value.type != Value::Type::Identifier) {
        return;
    }
    if (value.ident == ValueNames::NoWrap) {
        style.flex_wrap = ComputedStyle::FlexWrap::NoWrap;
    } else if (value.ident == ValueNames::Wrap) {
        style.flex_wrap = ComputedStyle::FlexWrap::Wrap;
    } else if (value.ident == ValueNames::WrapReverse) {
        style.flex_wrap = ComputedStyle::FlexWrap::WrapReverse;
    }
}

void apply_justify_content_value(ComputedStyle& style, const Value& value) {
    if (value.type != Value::Type::Identifier) {
        return;
    }
    const auto& ident = value.ident;
    if (ident == ValueNames::FlexStart || ident == ValueNames::Start || ident == ValueNames::Left) {
        style.justify_content = ComputedStyle::JustifyContent::FlexStart;
    } else if (ident == ValueNames::FlexEnd || ident == ValueNames::End || ident == ValueNames::Right) {
        style.justify_content = ComputedStyle::JustifyContent::FlexEnd;
    } else if (ident == ValueNames::Center) {
        style.justify_content = ComputedStyle::JustifyContent::Center;
    } else if (ident == ValueNames::SpaceBetween) {
        style.justify_content = ComputedStyle::JustifyContent::SpaceBetween;
    } else if (ident == ValueNames::SpaceAround) {
        style.justify_content = ComputedStyle::JustifyContent::SpaceAround;
    } else if (ident == ValueNames::SpaceEvenly) {
        style.justify_content = ComputedStyle::JustifyContent::SpaceEvenly;
    }
}

void apply_align_items_value(ComputedStyle& style, const Value& value) {
    if (value.type != Value::Type::Identifier) {
        return;
    }
    const auto& ident = value.ident;
    if (ident == ValueNames::Stretch || ident == ValueNames::Normal) {
        style.align_items = ComputedStyle::AlignItems::Stretch;
    } else if (ident == ValueNames::FlexStart || ident == ValueNames::Start) {
        style.align_items = ComputedStyle::AlignItems::FlexStart;
    } else if (ident == ValueNames::FlexEnd || ident == ValueNames::End) {
        style.align_items = ComputedStyle::AlignItems::FlexEnd;
    } else if (ident == ValueNames::Center) {
        style.align_items = ComputedStyle::AlignItems::Center;
    } else if (ident == ValueNames::Baseline) {
        style.align_items = ComputedStyle::AlignItems::Baseline;
    }
}

void apply_flex_factor_value(float& target, const Value& value, float fallback) {
    if (value.type == Value::Type::Number) {
        target = std::max(0.0f, value.number);
    } else if (value.type == Value::Type::Length && value.length.unit == Unit::Unknown) {
        target = std::max(0.0f, value.length.value);
    } else {
        target = fallback;
    }
}

void apply_order_value(ComputedStyle& style, const Value& value) {
    if (value.type == Value::Type::Number) {
        style.order = static_cast<int>(value.number);
    }
}

}  // namespace

bool apply_layout_property(Property property, const Value& value, ComputedStyle& style,
                           StyleDefaults::StyleOverrides& overrides, Context& context) {
    switch (property) {
        case Property::Display:
            if (apply_display_value(style, value) && context.display_set) {
                *context.display_set = true;
            }
            return true;
        case Property::Position:
            apply_position_value(style, value);
            return true;
        case Property::Overflow:
            if (auto overflow = parse_overflow_value(value)) {
                style.overflow_x = *overflow;
                style.overflow_y = *overflow;
            }
            return true;
        case Property::OverflowY:
            if (auto overflow = parse_overflow_value(value)) {
                style.overflow_y = *overflow;
            }
            return true;
        case Property::Margin:
            apply_edge(style.margin, StyleValueUtils::value_to_length(value, 0.0f, style.font_size));
            return true;
        case Property::MarginTop:
            apply_margin_value(style.margin.top, style.margin_top_auto, value, style.font_size);
            return true;
        case Property::MarginRight:
            apply_margin_value(style.margin.right, style.margin_right_auto, value, style.font_size);
            return true;
        case Property::MarginBottom:
            apply_margin_value(style.margin.bottom, style.margin_bottom_auto, value, style.font_size);
            return true;
        case Property::MarginLeft:
            apply_margin_value(style.margin.left, style.margin_left_auto, value, style.font_size);
            return true;
        case Property::Padding:
            apply_edge(style.padding, StyleValueUtils::value_to_length(value, 0.0f, style.font_size));
            return true;
        case Property::PaddingTop:
            apply_length(style.padding.top, value, style.font_size);
            return true;
        case Property::PaddingRight:
            apply_length(style.padding.right, value, style.font_size);
            return true;
        case Property::PaddingBottom:
            apply_length(style.padding.bottom, value, style.font_size);
            return true;
        case Property::PaddingLeft:
            apply_length(style.padding.left, value, style.font_size);
            return true;
        case Property::BoxSizing:
            if (value.type == Value::Type::Identifier) {
                if (value.ident == ValueNames::BorderBox) {
                    style.box_sizing = ComputedStyle::BoxSizing::BorderBox;
                } else if (value.ident == ValueNames::ContentBox) {
                    style.box_sizing = ComputedStyle::BoxSizing::ContentBox;
                }
            }
            return true;
        case Property::Transform:
            if (value.type == Value::Type::Identifier) {
                auto translate = parse_transform_translate(value.ident, style.font_size);
                if (translate) {
                    style.transform_has_translate = true;
                    style.transform_translate_x = translate->first;
                    style.transform_translate_y = translate->second;
                } else {
                    style.transform_has_translate = false;
                    style.transform_translate_x = 0.0f;
                    style.transform_translate_y = 0.0f;
                }
            }
            return true;
        case Property::BorderWidth:
            apply_edge(style.border_width, StyleValueUtils::value_to_length(value, 0.0f, style.font_size));
            return true;
        case Property::BorderTopWidth:
            style.border_width.top = StyleValueUtils::value_to_length(value, style.border_width.top, style.font_size);
            return true;
        case Property::BorderRightWidth:
            style.border_width.right =
                StyleValueUtils::value_to_length(value, style.border_width.right, style.font_size);
            return true;
        case Property::BorderBottomWidth:
            style.border_width.bottom =
                StyleValueUtils::value_to_length(value, style.border_width.bottom, style.font_size);
            return true;
        case Property::BorderLeftWidth:
            style.border_width.left = StyleValueUtils::value_to_length(value, style.border_width.left, style.font_size);
            return true;
        case Property::BorderRadius:
            // Shorthand normally expands to the four corner longhands in the
            // parser; handle a direct value defensively as all-corners.
            style.border_radius.set_all(value_to_corner_radius(value, style.font_size));
            return true;
        case Property::BorderTopLeftRadius:
            style.border_radius.top_left = value_to_corner_radius(value, style.font_size);
            return true;
        case Property::BorderTopRightRadius:
            style.border_radius.top_right = value_to_corner_radius(value, style.font_size);
            return true;
        case Property::BorderBottomRightRadius:
            style.border_radius.bottom_right = value_to_corner_radius(value, style.font_size);
            return true;
        case Property::BorderBottomLeftRadius:
            style.border_radius.bottom_left = value_to_corner_radius(value, style.font_size);
            return true;
        case Property::BorderColor:
            if (value.type == Value::Type::Color) {
                style.border_color = value.color;
                style.border_edge_color = {value.color, value.color, value.color, value.color};
            }
            return true;
        case Property::BorderTopColor:
            if (value.type == Value::Type::Color) {
                style.border_edge_color.top = value.color;
            }
            return true;
        case Property::BorderRightColor:
            if (value.type == Value::Type::Color) {
                style.border_edge_color.right = value.color;
            }
            return true;
        case Property::BorderBottomColor:
            if (value.type == Value::Type::Color) {
                style.border_edge_color.bottom = value.color;
            }
            return true;
        case Property::BorderLeftColor:
            if (value.type == Value::Type::Color) {
                style.border_edge_color.left = value.color;
            }
            return true;
        case Property::Outline:
            apply_outline_shorthand(style, value);
            return true;
        case Property::OutlineWidth:
            style.outline_width =
                std::max(0.0f, StyleValueUtils::value_to_length(value, style.outline_width, style.font_size));
            return true;
        case Property::OutlineColor:
            if (value.type == Value::Type::Color) {
                style.outline_color = value.color;
            }
            return true;
        case Property::OutlineOffset:
            style.outline_offset = StyleValueUtils::value_to_length(value, style.outline_offset, style.font_size);
            return true;
        case Property::BorderStyle:
            apply_border_style(style, value);
            return true;
        case Property::Width:
            apply_optional_length(style.width, style.width_is_percent, value, style.font_size);
            return true;
        case Property::Height:
            apply_optional_length(style.height, style.height_is_percent, value, style.font_size);
            return true;
        case Property::MinWidth:
            apply_optional_length(style.min_width, style.min_width_is_percent, value, style.font_size);
            return true;
        case Property::MinHeight:
            apply_optional_length(style.min_height, style.min_height_is_percent, value, style.font_size);
            return true;
        case Property::MaxWidth:
            apply_optional_length(style.max_width, style.max_width_is_percent, value, style.font_size);
            return true;
        case Property::MaxHeight:
            apply_optional_length(style.max_height, style.max_height_is_percent, value, style.font_size);
            return true;
        case Property::Top:
            apply_optional_length(style.top, style.top_is_percent, value, style.font_size);
            return true;
        case Property::Right:
            apply_optional_length(style.right, style.right_is_percent, value, style.font_size);
            return true;
        case Property::Bottom:
            apply_optional_length(style.bottom, style.bottom_is_percent, value, style.font_size);
            return true;
        case Property::Left:
            apply_optional_length(style.left, style.left_is_percent, value, style.font_size);
            return true;
        case Property::ZIndex:
            apply_z_index_value(style, value);
            return true;
        case Property::Opacity:
            apply_opacity_value(style, value);
            return true;
        case Property::FlexDirection:
            apply_flex_direction_value(style, value);
            return true;
        case Property::FlexWrap:
            apply_flex_wrap_value(style, value);
            return true;
        case Property::JustifyContent:
            apply_justify_content_value(style, value);
            return true;
        case Property::AlignItems:
            apply_align_items_value(style, value);
            return true;
        case Property::FlexGrow:
            apply_flex_factor_value(style.flex_grow, value, 0.0f);
            return true;
        case Property::FlexShrink:
            apply_flex_factor_value(style.flex_shrink, value, 1.0f);
            return true;
        case Property::FlexBasis:
            if (value.type == Value::Type::Identifier && value.ident == ValueNames::Auto) {
                style.flex_basis.reset();
                style.flex_basis_is_percent = false;
                return true;
            }
            apply_optional_length(style.flex_basis, style.flex_basis_is_percent, value, style.font_size);
            return true;
        case Property::Order:
            apply_order_value(style, value);
            return true;
        case Property::Flex:
            // Shorthand is expanded at parse time; nothing to apply directly.
            return true;
        case Property::Border:
            return true;
        case Property::BoxShadow:
            if (value.type == Value::Type::Identifier && value.ident == ValueNames::None) {
                style.box_shadow.reset();
                return true;
            }
            if (value.type == Value::Type::Shadow) {
                auto to_px = [&](const Length& length) {
                    if (length.unit == Unit::Px) return length.value;
                    if (length.unit == Unit::Em) return length.value * style.font_size;
                    return 0.0f;
                };
                ComputedStyle::BoxShadow shadow;
                shadow.offset_x = to_px(value.shadow.offset_x);
                shadow.offset_y = to_px(value.shadow.offset_y);
                shadow.blur = std::max(0.0f, to_px(value.shadow.blur));
                shadow.color = value.shadow.color;
                style.box_shadow = shadow;
            }
            return true;
        case Property::TextShadow:
            // text-shadow is inherited, so mark the override either way (including
            // `none`) to stop a hidden parent shadow from leaking back in.
            overrides.text_shadow = true;
            if (value.type == Value::Type::Identifier && value.ident == ValueNames::None) {
                style.text_shadow.reset();
                return true;
            }
            if (value.type == Value::Type::Shadow) {
                auto to_px = [&](const Length& length) {
                    if (length.unit == Unit::Px) return length.value;
                    if (length.unit == Unit::Em) return length.value * style.font_size;
                    return 0.0f;
                };
                ComputedStyle::BoxShadow shadow;
                shadow.offset_x = to_px(value.shadow.offset_x);
                shadow.offset_y = to_px(value.shadow.offset_y);
                shadow.blur = std::max(0.0f, to_px(value.shadow.blur));
                shadow.color = value.shadow.color;
                style.text_shadow = shadow;
            }
            return true;
        case Property::Clip: {
            // `clip: auto` (or any identifier) clears the clip.
            if (value.type != Value::Type::Clip) {
                style.clip.reset();
                return true;
            }
            auto to_px = [&](const std::optional<Length>& edge) -> std::optional<float> {
                if (!edge) return std::nullopt;
                if (edge->unit == Unit::Em) return edge->value * style.font_size;
                return edge->value;  // px (and unitless treated as px at parse)
            };
            ComputedStyle::ClipRect rect;
            rect.top = to_px(value.clip.top);
            rect.right = to_px(value.clip.right);
            rect.bottom = to_px(value.clip.bottom);
            rect.left = to_px(value.clip.left);
            style.clip = rect;
            return true;
        }
        default:
            return false;
    }
}

}  // namespace Hummingbird::Css::Apply
