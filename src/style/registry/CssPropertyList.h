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
    parse_font_shorthand,
    parse_length_number,
    parse_margin_shorthand,
    parse_length_auto,
    parse_padding_shorthand,
    parse_length,
    parse_transform,
    parse_border_shorthand,
    parse_border_radius,
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
    parse_outline_shorthand,
    parse_opacity,
    parse_box_shadow,
    parse_flex_shorthand,
};

enum class ApplyHook : std::uint8_t {
    Unknown,
    apply_display,
    apply_position,
    apply_overflow,
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
    apply_border_radius,
    apply_border_color,
    apply_outline,
    apply_outline_width,
    apply_outline_color,
    apply_outline_offset,
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
    apply_opacity,
    apply_text_align,
    apply_text_transform,
    apply_cursor,
    apply_visibility,
    apply_pointer_events,
    apply_vertical_align,
    apply_letter_spacing,
    apply_text_indent,
    apply_text_overflow,
    apply_word_wrap,
    apply_text_decoration,
    apply_text_decoration_thickness,
    apply_text_underline_offset,
    apply_white_space,
    apply_font_family,
    apply_font_weight,
    apply_font_style,
    apply_float,
    apply_clear,
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
    apply_box_shadow,
    apply_flex_direction,
    apply_flex_wrap,
    apply_justify_content,
    apply_align_items,
    apply_flex_grow,
    apply_flex_shrink,
    apply_flex_basis,
    apply_order,
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

namespace detail {

constexpr size_t kPropertyListCount = sizeof(kPropertyList) / sizeof(kPropertyList[0]);

constexpr bool entry_fields_valid() {
    for (const auto& entry : kPropertyList) {
        if (entry.property == Property::Unknown || entry.property == Property::Custom) {
            return false;
        }
        if (entry.name.empty() || entry.canonical_name.empty()) {
            return false;
        }
        if (entry.parser_hook == ParserHook::Unknown || entry.applier_hook == ApplyHook::Unknown) {
            return false;
        }
    }
    return true;
}

constexpr bool unique_property_names() {
    for (size_t i = 0; i < kPropertyListCount; ++i) {
        for (size_t j = i + 1; j < kPropertyListCount; ++j) {
            if (kPropertyList[i].name == kPropertyList[j].name) {
                return false;
            }
        }
    }
    return true;
}

constexpr bool exactly_one_canonical_per_property() {
    for (size_t i = 0; i < kPropertyListCount; ++i) {
        const auto& candidate = kPropertyList[i];
        if (candidate.name != candidate.canonical_name) {
            continue;
        }

        size_t canonical_count = 0;
        for (size_t j = 0; j < kPropertyListCount; ++j) {
            const auto& current = kPropertyList[j];
            if (current.property == candidate.property && current.name == current.canonical_name) {
                ++canonical_count;
            }
        }
        if (canonical_count != 1) {
            return false;
        }
    }

    for (const auto& entry : kPropertyList) {
        bool has_canonical = false;
        for (const auto& current : kPropertyList) {
            if (current.property == entry.property && current.name == current.canonical_name) {
                has_canonical = true;
                break;
            }
        }
        if (!has_canonical) {
            return false;
        }
    }
    return true;
}

constexpr bool alias_targets_canonical_entry() {
    for (const auto& entry : kPropertyList) {
        if (entry.name == entry.canonical_name) {
            continue;
        }

        bool target_found = false;
        for (const auto& target : kPropertyList) {
            if (target.name != entry.canonical_name) {
                continue;
            }
            if (target.name != target.canonical_name) {
                return false;
            }
            if (target.property != entry.property) {
                return false;
            }
            target_found = true;
            break;
        }
        if (!target_found) {
            return false;
        }
    }
    return true;
}

}  // namespace detail

static_assert(detail::entry_fields_valid(), "CssPropertyList has invalid property metadata.");
static_assert(detail::unique_property_names(), "CssPropertyList contains duplicate property names.");
static_assert(detail::exactly_one_canonical_per_property(),
              "CssPropertyList must define exactly one canonical entry for each property.");
static_assert(detail::alias_targets_canonical_entry(),
              "CssPropertyList aliases must target canonical entries for the same property.");

}  // namespace Hummingbird::Css::PropertyRegistry
