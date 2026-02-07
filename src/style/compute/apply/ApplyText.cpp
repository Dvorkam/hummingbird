#include "style/compute/apply/ApplyText.h"

#include "style/compute/StyleValueUtils.h"
#include "style/compute/apply/ApplyColorUtils.h"
#include "style/registry/CssValueNames.h"

namespace Hummingbird::Css::Apply {

namespace {
void apply_list_style_token(std::string_view token, ComputedStyle& style, StyleDefaults::StyleOverrides& overrides) {
    if (token == ValueNames::None) {
        style.list_style_type = ComputedStyle::ListStyleType::None;
        overrides.list_style_type = true;
        return;
    }
    if (token == ValueNames::Disc) {
        style.list_style_type = ComputedStyle::ListStyleType::Disc;
        overrides.list_style_type = true;
        return;
    }
    if (token == ValueNames::Inside) {
        style.list_style_position = ComputedStyle::ListStylePosition::Inside;
        overrides.list_style_position = true;
        return;
    }
    if (token == ValueNames::Outside) {
        style.list_style_position = ComputedStyle::ListStylePosition::Outside;
        overrides.list_style_position = true;
        return;
    }
}

void apply_color_value(const Value& value, ComputedStyle& style, StyleDefaults::StyleOverrides& overrides,
                       const ComputedStyle* parent_style) {
    if (value.type == Value::Type::Color) {
        style.color = value.color;
        overrides.color = true;
    } else if (value.type == Value::Type::Identifier) {
        if (auto resolved = resolve_var_color(style, parent_style, value.ident)) {
            style.color = *resolved;
            overrides.color = true;
        }
    }
}

}  // namespace

bool apply_text_property(Property property, const Value& value, ComputedStyle& style,
                         StyleDefaults::StyleOverrides& overrides, Context& context) {
    switch (property) {
        case Property::FontSize:
            if (value.type == Value::Type::Length) {
                if (value.length.unit == Unit::Px) {
                    style.font_size = value.length.value;
                    overrides.font_size = true;
                } else if (value.length.unit == Unit::Em) {
                    style.font_size = value.length.value * context.parent_font_size;
                    overrides.font_size = true;
                }
            }
            return true;
        case Property::LineHeight:
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
            return true;
        case Property::TextAlign:
            if (value.type == Value::Type::Identifier) {
                if (value.ident == ValueNames::Left) {
                    style.text_align = ComputedStyle::TextAlign::Left;
                    overrides.text_align = true;
                } else if (value.ident == ValueNames::Center) {
                    style.text_align = ComputedStyle::TextAlign::Center;
                    overrides.text_align = true;
                } else if (value.ident == ValueNames::Right) {
                    style.text_align = ComputedStyle::TextAlign::Right;
                    overrides.text_align = true;
                }
            }
            return true;
        case Property::TextDecoration:
            if (value.type == Value::Type::Identifier) {
                if (value.ident == ValueNames::Underline) {
                    style.underline = true;
                    overrides.underline = true;
                } else if (value.ident == ValueNames::None) {
                    style.underline = false;
                    overrides.underline = true;
                }
            }
            return true;
        case Property::TextDecorationThickness: {
            auto thickness = StyleValueUtils::value_to_length(value, 0.0f, style.font_size);
            if (thickness > 0.0f) {
                style.underline_thickness = thickness;
                overrides.underline_thickness = true;
            }
            return true;
        }
        case Property::TextUnderlineOffset: {
            auto offset = StyleValueUtils::value_to_length(value, 0.0f, style.font_size);
            if (offset > 0.0f) {
                style.underline_offset = offset;
                overrides.underline_offset = true;
            }
            return true;
        }
        case Property::WhiteSpace:
            if (value.type == Value::Type::Identifier) {
                if (value.ident == ValueNames::Normal) {
                    style.whitespace = ComputedStyle::WhiteSpace::Normal;
                    overrides.whitespace = true;
                } else if (value.ident == ValueNames::NoWrap) {
                    style.whitespace = ComputedStyle::WhiteSpace::NoWrap;
                    overrides.whitespace = true;
                }
            }
            return true;
        case Property::FontFamily:
            if (value.type == Value::Type::Identifier) {
                style.font_face = value.ident;
                overrides.font_face = true;
            }
            return true;
        case Property::FontWeight:
            if (value.type == Value::Type::Identifier) {
                if (value.ident == ValueNames::Bold) {
                    style.weight = ComputedStyle::FontWeight::Bold;
                    overrides.weight = true;
                } else if (value.ident == ValueNames::Normal) {
                    style.weight = ComputedStyle::FontWeight::Normal;
                    overrides.weight = true;
                }
            } else if (value.type == Value::Type::Number) {
                style.weight = value.number >= 600.0f ? ComputedStyle::FontWeight::Bold
                                                      : ComputedStyle::FontWeight::Normal;
                overrides.weight = true;
            }
            return true;
        case Property::FontStyle:
            if (value.type == Value::Type::Identifier) {
                if (value.ident == ValueNames::Italic) {
                    style.style = ComputedStyle::FontStyle::Italic;
                    overrides.style = true;
                } else if (value.ident == ValueNames::Normal) {
                    style.style = ComputedStyle::FontStyle::Normal;
                    overrides.style = true;
                }
            }
            return true;
        case Property::Float:
            if (value.type == Value::Type::Identifier) {
                if (value.ident == ValueNames::Left) {
                    style.float_type = ComputedStyle::Float::Left;
                } else if (value.ident == ValueNames::Right) {
                    style.float_type = ComputedStyle::Float::Right;
                } else if (value.ident == ValueNames::None) {
                    style.float_type = ComputedStyle::Float::None;
                }
            }
            return true;
        case Property::ListStyle:
            if (value.type == Value::Type::Identifier) {
                auto tokens = StyleValueUtils::split_tokens(value.ident);
                for (auto token : tokens) {
                    apply_list_style_token(token, style, overrides);
                }
            }
            return true;
        case Property::ListStyleType:
            if (value.type == Value::Type::Identifier) {
                apply_list_style_token(value.ident, style, overrides);
            }
            return true;
        case Property::ListStylePosition:
            if (value.type == Value::Type::Identifier) {
                apply_list_style_token(value.ident, style, overrides);
            }
            return true;
        case Property::Color:
            apply_color_value(value, style, overrides, context.parent_style);
            return true;
        default:
            return false;
    }
}

}  // namespace Hummingbird::Css::Apply
