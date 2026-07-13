#include "style/compute/apply/ApplyBackground.h"

#include <string_view>

#include "style/compute/StyleValueUtils.h"
#include "style/compute/apply/ApplyColorUtils.h"
#include "style/registry/CssValueNames.h"

namespace Hummingbird::Css::Apply {

namespace {
void apply_background_color_value(const Value& value, ComputedStyle& style, StyleDefaults::StyleOverrides& overrides,
                                  const ComputedStyle* parent_style) {
    if (value.type == Value::Type::Color) {
        style.background = value.color;
        overrides.background = true;
    } else if (value.type == Value::Type::Identifier) {
        if (value.ident == ValueNames::None || value.ident == ValueNames::Transparent) {
            style.background.reset();
            overrides.background = true;
            return;
        }
        if (auto resolved = resolve_var_color(style, parent_style, value.ident)) {
            style.background = *resolved;
            overrides.background = true;
        }
    }
}

void apply_background_image_value(const Value& value, ComputedStyle& style) {
    if (value.type == Value::Type::Url) {
        style.background_image = value.ident;
        return;
    }
    if (value.type == Value::Type::Identifier && value.ident == ValueNames::None) {
        style.background_image.reset();
    }
}

void apply_background_repeat_value(const Value& value, ComputedStyle& style) {
    if (value.type != Value::Type::Identifier) {
        return;
    }
    auto tokens = StyleValueUtils::split_tokens(value.ident);
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

void apply_background_position_value(const Value& value, ComputedStyle& style) {
    if (value.type != Value::Type::Identifier) {
        return;
    }
    auto tokens = StyleValueUtils::split_tokens(value.ident);
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
        if (auto length = StyleValueUtils::parse_length_token(tokens[index], font_size)) {
            style.background_position.offset_x = *length;
            horizontal_set = true;
        } else {
            apply_keyword(tokens[index]);
        }
        ++index;
    }
    if (index < tokens.size()) {
        if (auto length = StyleValueUtils::parse_length_token(tokens[index], font_size)) {
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

void apply_background_size_value(const Value& value, ComputedStyle& style) {
    if (value.type != Value::Type::Identifier) {
        return;
    }
    auto tokens = StyleValueUtils::split_tokens(value.ident);
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
    auto width = StyleValueUtils::parse_length_token(tokens[0], font_size);
    if (!width) {
        return;
    }
    style.background_size.type = ComputedStyle::BackgroundSize::Type::Length;
    style.background_size.width = *width;
    style.background_size.height.reset();
    if (tokens.size() > 1) {
        if (auto height = StyleValueUtils::parse_length_token(tokens[1], font_size)) {
            style.background_size.height = *height;
        }
    }
}

}  // namespace

bool apply_background_property(Property property, const Value& value, ComputedStyle& style,
                               StyleDefaults::StyleOverrides& overrides, Context& context) {
    switch (property) {
        case Property::BackgroundColor:
            apply_background_color_value(value, style, overrides, context.parent_style);
            return true;
        case Property::BackgroundImage:
            apply_background_image_value(value, style);
            return true;
        case Property::BackgroundRepeat:
            apply_background_repeat_value(value, style);
            return true;
        case Property::BackgroundPosition:
            apply_background_position_value(value, style);
            return true;
        case Property::BackgroundSize:
            apply_background_size_value(value, style);
            return true;
        case Property::Background:
            return true;
        default:
            return false;
    }
}

}  // namespace Hummingbird::Css::Apply
