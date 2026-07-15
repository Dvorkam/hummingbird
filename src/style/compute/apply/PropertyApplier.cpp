#include "style/compute/apply/PropertyApplier.h"

#include "style/compute/apply/ApplyBackground.h"
#include "style/compute/apply/ApplyLayout.h"
#include "style/compute/apply/ApplyText.h"
#include "style/registry/CssPropertyRegistry.h"

namespace Hummingbird::Css::Apply {

void apply_property(Property property, const Value& value, ComputedStyle& style,
                    StyleDefaults::StyleOverrides& overrides, Context& context) {
    using PropertyRegistry::ApplyHook;

    switch (PropertyRegistry::applier_hook(property)) {
        case ApplyHook::apply_display:
            (void)apply_layout_property(Property::Display, value, style, overrides, context);
            return;
        case ApplyHook::apply_position:
            (void)apply_layout_property(Property::Position, value, style, overrides, context);
            return;
        case ApplyHook::apply_overflow:
            (void)apply_layout_property(property, value, style, overrides, context);
            return;
        case ApplyHook::apply_font_size:
            (void)apply_text_property(Property::FontSize, value, style, overrides, context);
            return;
        case ApplyHook::apply_line_height:
            (void)apply_text_property(Property::LineHeight, value, style, overrides, context);
            return;
        case ApplyHook::apply_margin:
            (void)apply_layout_property(Property::Margin, value, style, overrides, context);
            return;
        case ApplyHook::apply_margin_top:
            (void)apply_layout_property(Property::MarginTop, value, style, overrides, context);
            return;
        case ApplyHook::apply_margin_right:
            (void)apply_layout_property(Property::MarginRight, value, style, overrides, context);
            return;
        case ApplyHook::apply_margin_bottom:
            (void)apply_layout_property(Property::MarginBottom, value, style, overrides, context);
            return;
        case ApplyHook::apply_margin_left:
            (void)apply_layout_property(Property::MarginLeft, value, style, overrides, context);
            return;
        case ApplyHook::apply_padding:
            (void)apply_layout_property(Property::Padding, value, style, overrides, context);
            return;
        case ApplyHook::apply_padding_top:
            (void)apply_layout_property(Property::PaddingTop, value, style, overrides, context);
            return;
        case ApplyHook::apply_padding_right:
            (void)apply_layout_property(Property::PaddingRight, value, style, overrides, context);
            return;
        case ApplyHook::apply_padding_bottom:
            (void)apply_layout_property(Property::PaddingBottom, value, style, overrides, context);
            return;
        case ApplyHook::apply_padding_left:
            (void)apply_layout_property(Property::PaddingLeft, value, style, overrides, context);
            return;
        case ApplyHook::apply_box_sizing:
            (void)apply_layout_property(Property::BoxSizing, value, style, overrides, context);
            return;
        case ApplyHook::apply_transform:
            (void)apply_layout_property(Property::Transform, value, style, overrides, context);
            return;
        case ApplyHook::apply_border:
            (void)apply_layout_property(property, value, style, overrides, context);
            return;
        case ApplyHook::apply_border_width:
            (void)apply_layout_property(property, value, style, overrides, context);
            return;
        case ApplyHook::apply_border_radius:
            // property carries the specific corner (or the shorthand) so the
            // layout applier can target one corner or all four.
            (void)apply_layout_property(property, value, style, overrides, context);
            return;
        case ApplyHook::apply_border_color:
            // property carries the specific side (or the shorthand).
            (void)apply_layout_property(property, value, style, overrides, context);
            return;
        case ApplyHook::apply_outline:
            (void)apply_layout_property(Property::Outline, value, style, overrides, context);
            return;
        case ApplyHook::apply_outline_width:
            (void)apply_layout_property(Property::OutlineWidth, value, style, overrides, context);
            return;
        case ApplyHook::apply_outline_color:
            (void)apply_layout_property(Property::OutlineColor, value, style, overrides, context);
            return;
        case ApplyHook::apply_outline_offset:
            (void)apply_layout_property(Property::OutlineOffset, value, style, overrides, context);
            return;
        case ApplyHook::apply_border_style:
            (void)apply_layout_property(Property::BorderStyle, value, style, overrides, context);
            return;
        case ApplyHook::apply_width:
            (void)apply_layout_property(Property::Width, value, style, overrides, context);
            return;
        case ApplyHook::apply_height:
            (void)apply_layout_property(Property::Height, value, style, overrides, context);
            return;
        case ApplyHook::apply_min_width:
            (void)apply_layout_property(Property::MinWidth, value, style, overrides, context);
            return;
        case ApplyHook::apply_min_height:
            (void)apply_layout_property(Property::MinHeight, value, style, overrides, context);
            return;
        case ApplyHook::apply_max_width:
            (void)apply_layout_property(Property::MaxWidth, value, style, overrides, context);
            return;
        case ApplyHook::apply_max_height:
            (void)apply_layout_property(Property::MaxHeight, value, style, overrides, context);
            return;
        case ApplyHook::apply_top:
            (void)apply_layout_property(Property::Top, value, style, overrides, context);
            return;
        case ApplyHook::apply_right:
            (void)apply_layout_property(Property::Right, value, style, overrides, context);
            return;
        case ApplyHook::apply_bottom:
            (void)apply_layout_property(Property::Bottom, value, style, overrides, context);
            return;
        case ApplyHook::apply_left:
            (void)apply_layout_property(Property::Left, value, style, overrides, context);
            return;
        case ApplyHook::apply_z_index:
            (void)apply_layout_property(Property::ZIndex, value, style, overrides, context);
            return;
        case ApplyHook::apply_opacity:
            (void)apply_layout_property(Property::Opacity, value, style, overrides, context);
            return;
        case ApplyHook::apply_text_align:
            (void)apply_text_property(Property::TextAlign, value, style, overrides, context);
            return;
        case ApplyHook::apply_text_transform:
            (void)apply_text_property(Property::TextTransform, value, style, overrides, context);
            return;
        case ApplyHook::apply_cursor:
            (void)apply_text_property(property, value, style, overrides, context);
            return;
        case ApplyHook::apply_visibility:
            (void)apply_text_property(Property::Visibility, value, style, overrides, context);
            return;
        case ApplyHook::apply_pointer_events:
            (void)apply_text_property(Property::PointerEvents, value, style, overrides, context);
            return;
        case ApplyHook::apply_vertical_align:
            (void)apply_text_property(property, value, style, overrides, context);
            return;
        case ApplyHook::apply_letter_spacing:
            (void)apply_text_property(Property::LetterSpacing, value, style, overrides, context);
            return;
        case ApplyHook::apply_text_indent:
            (void)apply_text_property(Property::TextIndent, value, style, overrides, context);
            return;
        case ApplyHook::apply_text_overflow:
            (void)apply_text_property(Property::TextOverflow, value, style, overrides, context);
            return;
        case ApplyHook::apply_word_wrap:
            (void)apply_text_property(Property::WordWrap, value, style, overrides, context);
            return;
        case ApplyHook::apply_text_decoration:
            (void)apply_text_property(Property::TextDecoration, value, style, overrides, context);
            return;
        case ApplyHook::apply_text_decoration_thickness:
            (void)apply_text_property(Property::TextDecorationThickness, value, style, overrides, context);
            return;
        case ApplyHook::apply_text_underline_offset:
            (void)apply_text_property(Property::TextUnderlineOffset, value, style, overrides, context);
            return;
        case ApplyHook::apply_white_space:
            (void)apply_text_property(Property::WhiteSpace, value, style, overrides, context);
            return;
        case ApplyHook::apply_font_family:
            (void)apply_text_property(Property::FontFamily, value, style, overrides, context);
            return;
        case ApplyHook::apply_font_weight:
            (void)apply_text_property(Property::FontWeight, value, style, overrides, context);
            return;
        case ApplyHook::apply_font_style:
            (void)apply_text_property(Property::FontStyle, value, style, overrides, context);
            return;
        case ApplyHook::apply_float:
            (void)apply_text_property(Property::Float, value, style, overrides, context);
            return;
        case ApplyHook::apply_clear:
            (void)apply_text_property(Property::Clear, value, style, overrides, context);
            return;
        case ApplyHook::apply_list_style:
            (void)apply_text_property(Property::ListStyle, value, style, overrides, context);
            return;
        case ApplyHook::apply_list_style_type:
            (void)apply_text_property(Property::ListStyleType, value, style, overrides, context);
            return;
        case ApplyHook::apply_list_style_position:
            (void)apply_text_property(Property::ListStylePosition, value, style, overrides, context);
            return;
        case ApplyHook::apply_color:
            (void)apply_text_property(Property::Color, value, style, overrides, context);
            return;
        case ApplyHook::apply_background_color:
            (void)apply_background_property(Property::BackgroundColor, value, style, overrides, context);
            return;
        case ApplyHook::apply_background:
            (void)apply_background_property(Property::Background, value, style, overrides, context);
            return;
        case ApplyHook::apply_background_image:
            (void)apply_background_property(Property::BackgroundImage, value, style, overrides, context);
            return;
        case ApplyHook::apply_background_repeat:
            (void)apply_background_property(Property::BackgroundRepeat, value, style, overrides, context);
            return;
        case ApplyHook::apply_background_position:
            (void)apply_background_property(Property::BackgroundPosition, value, style, overrides, context);
            return;
        case ApplyHook::apply_background_size:
            (void)apply_background_property(Property::BackgroundSize, value, style, overrides, context);
            return;
        case ApplyHook::apply_box_shadow:
            (void)apply_layout_property(property, value, style, overrides, context);
            return;
        case ApplyHook::apply_text_shadow:
            (void)apply_layout_property(Property::TextShadow, value, style, overrides, context);
            return;
        case ApplyHook::apply_clip:
            (void)apply_layout_property(Property::Clip, value, style, overrides, context);
            return;
        case ApplyHook::apply_flex_direction:
            (void)apply_layout_property(Property::FlexDirection, value, style, overrides, context);
            return;
        case ApplyHook::apply_flex_wrap:
            (void)apply_layout_property(Property::FlexWrap, value, style, overrides, context);
            return;
        case ApplyHook::apply_justify_content:
            (void)apply_layout_property(Property::JustifyContent, value, style, overrides, context);
            return;
        case ApplyHook::apply_align_items:
            (void)apply_layout_property(Property::AlignItems, value, style, overrides, context);
            return;
        case ApplyHook::apply_flex_grow:
            (void)apply_layout_property(property, value, style, overrides, context);
            return;
        case ApplyHook::apply_flex_shrink:
            (void)apply_layout_property(Property::FlexShrink, value, style, overrides, context);
            return;
        case ApplyHook::apply_flex_basis:
            (void)apply_layout_property(Property::FlexBasis, value, style, overrides, context);
            return;
        case ApplyHook::apply_order:
            (void)apply_layout_property(Property::Order, value, style, overrides, context);
            return;
        case ApplyHook::Unknown:
        case ApplyHook::Count:
            return;
    }
}

}  // namespace Hummingbird::Css::Apply
