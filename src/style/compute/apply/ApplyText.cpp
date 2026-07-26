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
    if (token == ValueNames::Decimal) {
        style.list_style_type = ComputedStyle::ListStyleType::Decimal;
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
    const StyleValueUtils::LengthResolutionContext length_context{style.font_size, context.root_font_size};
    switch (property) {
        case Property::FontSize:
            if (value.type == Value::Type::Length) {
                if (value.length.unit == Unit::Px) {
                    style.font_size = value.length.value;
                    overrides.font_size = true;
                } else if (value.length.unit == Unit::Em) {
                    style.font_size = value.length.value * context.parent_font_size;
                    overrides.font_size = true;
                } else if (value.length.unit == Unit::Rem) {
                    style.font_size = value.length.value * context.root_font_size;
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
            } else if (value.type == Value::Type::Length && value.length.unit == Unit::Rem) {
                style.line_height = value.length.value * context.root_font_size;
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
        case Property::TextTransform:
            if (value.type == Value::Type::Identifier) {
                if (value.ident == ValueNames::Uppercase) {
                    style.text_transform = ComputedStyle::TextTransform::Uppercase;
                    overrides.text_transform = true;
                } else if (value.ident == ValueNames::Lowercase) {
                    style.text_transform = ComputedStyle::TextTransform::Lowercase;
                    overrides.text_transform = true;
                } else if (value.ident == ValueNames::Capitalize) {
                    style.text_transform = ComputedStyle::TextTransform::Capitalize;
                    overrides.text_transform = true;
                } else if (value.ident == ValueNames::None) {
                    style.text_transform = ComputedStyle::TextTransform::None;
                    overrides.text_transform = true;
                }
            }
            return true;
        case Property::Cursor:
            if (value.type == Value::Type::Identifier) {
                if (value.ident == ValueNames::Auto) {
                    style.cursor = ComputedStyle::Cursor::Auto;
                    overrides.cursor = true;
                } else if (value.ident == ValueNames::Default) {
                    style.cursor = ComputedStyle::Cursor::Default;
                    overrides.cursor = true;
                } else if (value.ident == ValueNames::Pointer) {
                    style.cursor = ComputedStyle::Cursor::Pointer;
                    overrides.cursor = true;
                } else if (value.ident == ValueNames::Text) {
                    style.cursor = ComputedStyle::Cursor::Text;
                    overrides.cursor = true;
                }
            }
            return true;
        case Property::Visibility:
            if (value.type == Value::Type::Identifier) {
                if (value.ident == ValueNames::Visible) {
                    style.visibility = ComputedStyle::Visibility::Visible;
                    overrides.visibility = true;
                } else if (value.ident == ValueNames::Hidden) {
                    style.visibility = ComputedStyle::Visibility::Hidden;
                    overrides.visibility = true;
                } else if (value.ident == ValueNames::Collapse) {
                    style.visibility = ComputedStyle::Visibility::Collapse;
                    overrides.visibility = true;
                }
            }
            return true;
        case Property::PointerEvents:
            if (value.type == Value::Type::Identifier) {
                if (value.ident == ValueNames::Auto) {
                    style.pointer_events = ComputedStyle::PointerEvents::Auto;
                    overrides.pointer_events = true;
                } else if (value.ident == ValueNames::None) {
                    style.pointer_events = ComputedStyle::PointerEvents::None;
                    overrides.pointer_events = true;
                }
            }
            return true;
        case Property::VerticalAlign:
            if (value.type == Value::Type::Identifier) {
                if (value.ident == ValueNames::Baseline) {
                    style.vertical_align = ComputedStyle::VerticalAlign::Baseline;
                } else if (value.ident == ValueNames::Top) {
                    style.vertical_align = ComputedStyle::VerticalAlign::Top;
                } else if (value.ident == ValueNames::Middle) {
                    style.vertical_align = ComputedStyle::VerticalAlign::Middle;
                } else if (value.ident == ValueNames::Bottom) {
                    style.vertical_align = ComputedStyle::VerticalAlign::Bottom;
                }
            }
            return true;
        case Property::LetterSpacing:
            if (value.type == Value::Type::Identifier && value.ident == ValueNames::Normal) {
                style.letter_spacing = 0.0f;
                overrides.letter_spacing = true;
                return true;
            }
            style.letter_spacing = StyleValueUtils::value_to_length(value, style.letter_spacing, length_context);
            overrides.letter_spacing = true;
            return true;
        case Property::TextIndent:
            style.text_indent = StyleValueUtils::value_to_length(value, style.text_indent, length_context);
            overrides.text_indent = true;
            return true;
        case Property::TextOverflow:
            if (value.type == Value::Type::Identifier) {
                if (value.ident == ValueNames::Ellipsis) {
                    style.text_overflow = ComputedStyle::TextOverflow::Ellipsis;
                } else {
                    style.text_overflow = ComputedStyle::TextOverflow::Clip;
                }
            }
            return true;
        case Property::WordWrap:
            if (value.type == Value::Type::Identifier) {
                if (value.ident == ValueNames::BreakWord) {
                    style.word_wrap = ComputedStyle::WordWrap::BreakWord;
                } else {
                    style.word_wrap = ComputedStyle::WordWrap::Normal;
                }
                overrides.word_wrap = true;
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
            auto thickness = StyleValueUtils::value_to_length(value, 0.0f, length_context);
            if (thickness > 0.0f) {
                style.underline_thickness = thickness;
                overrides.underline_thickness = true;
            }
            return true;
        }
        case Property::TextUnderlineOffset: {
            auto offset = StyleValueUtils::value_to_length(value, 0.0f, length_context);
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
                style.weight =
                    value.number >= 600.0f ? ComputedStyle::FontWeight::Bold : ComputedStyle::FontWeight::Normal;
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
        case Property::Clear:
            if (value.type == Value::Type::Identifier) {
                if (value.ident == ValueNames::Left) {
                    style.clear = ComputedStyle::Clear::Left;
                } else if (value.ident == ValueNames::Right) {
                    style.clear = ComputedStyle::Clear::Right;
                } else if (value.ident == ValueNames::Both) {
                    style.clear = ComputedStyle::Clear::Both;
                } else if (value.ident == ValueNames::None) {
                    style.clear = ComputedStyle::Clear::None;
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
