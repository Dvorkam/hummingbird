// Canonical property entries
HB_CSS_PROPERTY(Display, Display, "display", "display", ParserHook::parse_identifier, ApplyHook::apply_display,
                PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(Position, Position, "position", "position", ParserHook::parse_identifier, ApplyHook::apply_position,
                PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(FontSize, FontSize, "font-size", "font-size", ParserHook::parse_font_size, ApplyHook::apply_font_size,
                PropertyFlags::Inherited | PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(LineHeight, LineHeight, "line-height", "line-height", ParserHook::parse_length_number, ApplyHook::apply_line_height,
                PropertyFlags::Inherited | PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(Margin, Margin, "margin", "margin", ParserHook::parse_margin_shorthand, ApplyHook::apply_margin,
                PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(MarginTop, MarginTop, "margin-top", "margin-top", ParserHook::parse_length_auto, ApplyHook::apply_margin_top,
                PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(MarginRight, MarginRight, "margin-right", "margin-right", ParserHook::parse_length_auto, ApplyHook::apply_margin_right,
                PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(MarginBottom, MarginBottom, "margin-bottom", "margin-bottom", ParserHook::parse_length_auto,
                ApplyHook::apply_margin_bottom, PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(MarginLeft, MarginLeft, "margin-left", "margin-left", ParserHook::parse_length_auto, ApplyHook::apply_margin_left,
                PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(Padding, Padding, "padding", "padding", ParserHook::parse_padding_shorthand, ApplyHook::apply_padding,
                PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(PaddingTop, PaddingTop, "padding-top", "padding-top", ParserHook::parse_length, ApplyHook::apply_padding_top,
                PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(PaddingRight, PaddingRight, "padding-right", "padding-right", ParserHook::parse_length, ApplyHook::apply_padding_right,
                PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(PaddingBottom, PaddingBottom, "padding-bottom", "padding-bottom", ParserHook::parse_length,
                ApplyHook::apply_padding_bottom, PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(PaddingLeft, PaddingLeft, "padding-left", "padding-left", ParserHook::parse_length, ApplyHook::apply_padding_left,
                PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(BoxSizing, BoxSizing, "box-sizing", "box-sizing", ParserHook::parse_identifier, ApplyHook::apply_box_sizing,
                PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(Transform, Transform, "transform", "transform", ParserHook::parse_transform, ApplyHook::apply_transform,
                PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(Border, Border, "border", "border", ParserHook::parse_border_shorthand, ApplyHook::apply_border,
                PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(BorderWidth, BorderWidth, "border-width", "border-width", ParserHook::parse_length, ApplyHook::apply_border_width,
                PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(BorderColor, BorderColor, "border-color", "border-color", ParserHook::parse_color, ApplyHook::apply_border_color,
                PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(BorderStyle, BorderStyle, "border-style", "border-style", ParserHook::parse_identifier, ApplyHook::apply_border_style,
                PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(Width, Width, "width", "width", ParserHook::parse_length_auto, ApplyHook::apply_width, PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(Height, Height, "height", "height", ParserHook::parse_length_auto, ApplyHook::apply_height,
                PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(MinWidth, MinWidth, "min-width", "min-width", ParserHook::parse_length, ApplyHook::apply_min_width,
                PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(MinHeight, MinHeight, "min-height", "min-height", ParserHook::parse_length, ApplyHook::apply_min_height,
                PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(MaxWidth, MaxWidth, "max-width", "max-width", ParserHook::parse_length, ApplyHook::apply_max_width,
                PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(MaxHeight, MaxHeight, "max-height", "max-height", ParserHook::parse_length, ApplyHook::apply_max_height,
                PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(Top, Top, "top", "top", ParserHook::parse_length_auto, ApplyHook::apply_top, PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(Right, Right, "right", "right", ParserHook::parse_length_auto, ApplyHook::apply_right, PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(Bottom, Bottom, "bottom", "bottom", ParserHook::parse_length_auto, ApplyHook::apply_bottom,
                PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(Left, Left, "left", "left", ParserHook::parse_length_auto, ApplyHook::apply_left, PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(ZIndex, ZIndex, "z-index", "z-index", ParserHook::parse_number_auto, ApplyHook::apply_z_index,
                PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(TextAlign, TextAlign, "text-align", "text-align", ParserHook::parse_identifier, ApplyHook::apply_text_align,
                PropertyFlags::Inherited | PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(TextDecoration, TextDecoration, "text-decoration", "text-decoration", ParserHook::parse_text_decoration,
                ApplyHook::apply_text_decoration, PropertyFlags::Inherited)
HB_CSS_PROPERTY(TextDecorationThickness, TextDecorationThickness, "text-decoration-thickness",
                "text-decoration-thickness", ParserHook::parse_length_number, ApplyHook::apply_text_decoration_thickness,
                PropertyFlags::Inherited)
HB_CSS_PROPERTY(TextUnderlineOffset, TextUnderlineOffset, "text-underline-offset", "text-underline-offset",
                ParserHook::parse_length_number, ApplyHook::apply_text_underline_offset, PropertyFlags::Inherited)
HB_CSS_PROPERTY(WhiteSpace, WhiteSpace, "white-space", "white-space", ParserHook::parse_identifier, ApplyHook::apply_white_space,
                PropertyFlags::Inherited | PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(FontFamily, FontFamily, "font-family", "font-family", ParserHook::parse_font_family, ApplyHook::apply_font_family,
                PropertyFlags::Inherited | PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(FontWeight, FontWeight, "font-weight", "font-weight", ParserHook::parse_font_weight, ApplyHook::apply_font_weight,
                PropertyFlags::Inherited | PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(FontStyle, FontStyle, "font-style", "font-style", ParserHook::parse_identifier, ApplyHook::apply_font_style,
                PropertyFlags::Inherited | PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(Float, Float, "float", "float", ParserHook::parse_identifier, ApplyHook::apply_float, PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(ListStyle, ListStyle, "list-style", "list-style", ParserHook::parse_list_style_shorthand, ApplyHook::apply_list_style,
                PropertyFlags::Inherited | PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(ListStyleType, ListStyleType, "list-style-type", "list-style-type", ParserHook::parse_identifier,
                ApplyHook::apply_list_style_type, PropertyFlags::Inherited | PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(ListStylePosition, ListStylePosition, "list-style-position", "list-style-position",
                ParserHook::parse_identifier, ApplyHook::apply_list_style_position, PropertyFlags::Inherited | PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(Color, Color, "color", "color", ParserHook::parse_color, ApplyHook::apply_color,
                PropertyFlags::Inherited | PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(BackgroundColor, BackgroundColor, "background-color", "background-color", ParserHook::parse_color,
                ApplyHook::apply_background_color, PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(Background, Background, "background", "background", ParserHook::parse_background_shorthand, ApplyHook::apply_background,
                PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(BackgroundImage, BackgroundImage, "background-image", "background-image", ParserHook::parse_background_image,
                ApplyHook::apply_background_image, PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(BackgroundRepeat, BackgroundRepeat, "background-repeat", "background-repeat", ParserHook::parse_background_repeat,
                ApplyHook::apply_background_repeat, PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(BackgroundPosition, BackgroundPosition, "background-position", "background-position",
                ParserHook::parse_background_position, ApplyHook::apply_background_position, PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(BackgroundSize, BackgroundSize, "background-size", "background-size", ParserHook::parse_background_size,
                ApplyHook::apply_background_size, PropertyFlags::LayoutAffecting)

// Alias property names that map to canonical entries
HB_CSS_PROPERTY_ALIAS(BoxSizing, WebkitBoxSizing, "-webkit-box-sizing", "box-sizing", ParserHook::parse_identifier,
                      ApplyHook::apply_box_sizing, PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY_ALIAS(BoxSizing, MozBoxSizing, "-moz-box-sizing", "box-sizing", ParserHook::parse_identifier,
                      ApplyHook::apply_box_sizing, PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY_ALIAS(BoxSizing, MsBoxSizing, "-ms-box-sizing", "box-sizing", ParserHook::parse_identifier, ApplyHook::apply_box_sizing,
                      PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY_ALIAS(BoxSizing, OBoxSizing, "-o-box-sizing", "box-sizing", ParserHook::parse_identifier, ApplyHook::apply_box_sizing,
                      PropertyFlags::LayoutAffecting)
