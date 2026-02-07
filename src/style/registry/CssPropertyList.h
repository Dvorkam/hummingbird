#pragma once

#include <cstdint>
#include <span>
#include <string_view>

#include "style/compute/Stylesheet.h"

namespace Hummingbird::Css::PropertyRegistry {

enum class PropertyFlags : std::uint8_t {
    None = 0,
    Inherited = 1 << 0,
    LayoutAffecting = 1 << 1,
};

enum class ParserHook : std::uint8_t {
    Unknown,
    parse_identifier,
    parse_font_size,
    parse_length_number,
    parse_margin_shorthand,
    parse_length_auto,
    parse_padding_shorthand,
    parse_length,
    parse_transform,
    parse_border_shorthand,
    parse_color,
    parse_number_auto,
    parse_text_decoration,
    parse_font_family,
    parse_font_weight,
    parse_list_style_shorthand,
    parse_background_shorthand,
    parse_background_image,
    parse_background_repeat,
    parse_background_position,
    parse_background_size,
};

enum class ApplyHook : std::uint8_t {
    Unknown,
    apply_display,
    apply_position,
    apply_font_size,
    apply_line_height,
    apply_margin,
    apply_margin_top,
    apply_margin_right,
    apply_margin_bottom,
    apply_margin_left,
    apply_padding,
    apply_padding_top,
    apply_padding_right,
    apply_padding_bottom,
    apply_padding_left,
    apply_box_sizing,
    apply_transform,
    apply_border,
    apply_border_width,
    apply_border_color,
    apply_border_style,
    apply_width,
    apply_height,
    apply_min_width,
    apply_min_height,
    apply_max_width,
    apply_max_height,
    apply_top,
    apply_right,
    apply_bottom,
    apply_left,
    apply_z_index,
    apply_text_align,
    apply_text_decoration,
    apply_text_decoration_thickness,
    apply_text_underline_offset,
    apply_white_space,
    apply_font_family,
    apply_font_weight,
    apply_font_style,
    apply_float,
    apply_list_style,
    apply_list_style_type,
    apply_list_style_position,
    apply_color,
    apply_background_color,
    apply_background,
    apply_background_image,
    apply_background_repeat,
    apply_background_position,
    apply_background_size,
};

constexpr PropertyFlags operator|(PropertyFlags lhs, PropertyFlags rhs) {
    return static_cast<PropertyFlags>(static_cast<std::uint8_t>(lhs) | static_cast<std::uint8_t>(rhs));
}

constexpr bool has_flag(PropertyFlags value, PropertyFlags flag) {
    return (static_cast<std::uint8_t>(value) & static_cast<std::uint8_t>(flag)) != 0;
}

struct PropertyListEntry {
    Property property;
    std::string_view name;
    std::string_view canonical_name;
    ParserHook parser_hook;
    ApplyHook applier_hook;
    PropertyFlags flags = PropertyFlags::None;
};

// Phase 4/R4-01: central metadata list for CSS properties.
// Existing parser/apply paths are unchanged for now; this file is additive.
inline constexpr PropertyListEntry kPropertyList[] = {
#define HB_CSS_PROPERTY(property_id, name_id, css_name, canonical_name, parser_hook, applier_hook, flags) \
    {Property::property_id, css_name, canonical_name, parser_hook, applier_hook, flags},
#define HB_CSS_PROPERTY_ALIAS(property_id, name_id, css_name, canonical_name, parser_hook, applier_hook, flags) \
    {Property::property_id, css_name, canonical_name, parser_hook, applier_hook, flags},
#include "style/registry/CssPropertyList.inl"
#undef HB_CSS_PROPERTY_ALIAS
#undef HB_CSS_PROPERTY
};

inline std::span<const PropertyListEntry> property_list() {
    return kPropertyList;
}

}  // namespace Hummingbird::Css::PropertyRegistry
