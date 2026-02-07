// Canonical property entries
HB_CSS_PROPERTY(Display, Display, "display", "display", "parse_identifier", "apply_display",
                PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(Position, Position, "position", "position", "parse_identifier", "apply_position",
                PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(FontSize, FontSize, "font-size", "font-size", "parse_font_size", "apply_font_size",
                PropertyFlags::Inherited | PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(LineHeight, LineHeight, "line-height", "line-height", "parse_length_number", "apply_line_height",
                PropertyFlags::Inherited | PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(Margin, Margin, "margin", "margin", "parse_margin_shorthand", "apply_margin",
                PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(MarginTop, MarginTop, "margin-top", "margin-top", "parse_length_auto", "apply_margin_top",
                PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(MarginRight, MarginRight, "margin-right", "margin-right", "parse_length_auto", "apply_margin_right",
                PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(MarginBottom, MarginBottom, "margin-bottom", "margin-bottom", "parse_length_auto",
                "apply_margin_bottom", PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(MarginLeft, MarginLeft, "margin-left", "margin-left", "parse_length_auto", "apply_margin_left",
                PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(Padding, Padding, "padding", "padding", "parse_padding_shorthand", "apply_padding",
                PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(PaddingTop, PaddingTop, "padding-top", "padding-top", "parse_length", "apply_padding_top",
                PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(PaddingRight, PaddingRight, "padding-right", "padding-right", "parse_length", "apply_padding_right",
                PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(PaddingBottom, PaddingBottom, "padding-bottom", "padding-bottom", "parse_length",
                "apply_padding_bottom", PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(PaddingLeft, PaddingLeft, "padding-left", "padding-left", "parse_length", "apply_padding_left",
                PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(BoxSizing, BoxSizing, "box-sizing", "box-sizing", "parse_identifier", "apply_box_sizing",
                PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(Transform, Transform, "transform", "transform", "parse_transform", "apply_transform",
                PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(Border, Border, "border", "border", "parse_border_shorthand", "apply_border",
                PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(BorderWidth, BorderWidth, "border-width", "border-width", "parse_length", "apply_border_width",
                PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(BorderColor, BorderColor, "border-color", "border-color", "parse_color", "apply_border_color",
                PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(BorderStyle, BorderStyle, "border-style", "border-style", "parse_identifier", "apply_border_style",
                PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(Width, Width, "width", "width", "parse_length_auto", "apply_width", PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(Height, Height, "height", "height", "parse_length_auto", "apply_height",
                PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(MinWidth, MinWidth, "min-width", "min-width", "parse_length", "apply_min_width",
                PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(MinHeight, MinHeight, "min-height", "min-height", "parse_length", "apply_min_height",
                PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(MaxWidth, MaxWidth, "max-width", "max-width", "parse_length", "apply_max_width",
                PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(MaxHeight, MaxHeight, "max-height", "max-height", "parse_length", "apply_max_height",
                PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(Top, Top, "top", "top", "parse_length_auto", "apply_top", PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(Right, Right, "right", "right", "parse_length_auto", "apply_right", PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(Bottom, Bottom, "bottom", "bottom", "parse_length_auto", "apply_bottom",
                PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(Left, Left, "left", "left", "parse_length_auto", "apply_left", PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(ZIndex, ZIndex, "z-index", "z-index", "parse_number_auto", "apply_z_index",
                PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(TextAlign, TextAlign, "text-align", "text-align", "parse_identifier", "apply_text_align",
                PropertyFlags::Inherited | PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(TextDecoration, TextDecoration, "text-decoration", "text-decoration", "parse_text_decoration",
                "apply_text_decoration", PropertyFlags::Inherited)
HB_CSS_PROPERTY(TextDecorationThickness, TextDecorationThickness, "text-decoration-thickness",
                "text-decoration-thickness", "parse_length_number", "apply_text_decoration_thickness",
                PropertyFlags::Inherited)
HB_CSS_PROPERTY(TextUnderlineOffset, TextUnderlineOffset, "text-underline-offset", "text-underline-offset",
                "parse_length_number", "apply_text_underline_offset", PropertyFlags::Inherited)
HB_CSS_PROPERTY(WhiteSpace, WhiteSpace, "white-space", "white-space", "parse_identifier", "apply_white_space",
                PropertyFlags::Inherited | PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(FontFamily, FontFamily, "font-family", "font-family", "parse_font_family", "apply_font_family",
                PropertyFlags::Inherited | PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(FontWeight, FontWeight, "font-weight", "font-weight", "parse_font_weight", "apply_font_weight",
                PropertyFlags::Inherited | PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(FontStyle, FontStyle, "font-style", "font-style", "parse_identifier", "apply_font_style",
                PropertyFlags::Inherited | PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(Float, Float, "float", "float", "parse_identifier", "apply_float", PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(ListStyle, ListStyle, "list-style", "list-style", "parse_list_style_shorthand", "apply_list_style",
                PropertyFlags::Inherited | PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(ListStyleType, ListStyleType, "list-style-type", "list-style-type", "parse_identifier",
                "apply_list_style_type", PropertyFlags::Inherited | PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(ListStylePosition, ListStylePosition, "list-style-position", "list-style-position",
                "parse_identifier", "apply_list_style_position", PropertyFlags::Inherited | PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(Color, Color, "color", "color", "parse_color", "apply_color",
                PropertyFlags::Inherited | PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(BackgroundColor, BackgroundColor, "background-color", "background-color", "parse_color",
                "apply_background_color", PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(Background, Background, "background", "background", "parse_background_shorthand", "apply_background",
                PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(BackgroundImage, BackgroundImage, "background-image", "background-image", "parse_background_image",
                "apply_background_image", PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(BackgroundRepeat, BackgroundRepeat, "background-repeat", "background-repeat", "parse_identifier",
                "apply_background_repeat", PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(BackgroundPosition, BackgroundPosition, "background-position", "background-position",
                "parse_background_position", "apply_background_position", PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(BackgroundSize, BackgroundSize, "background-size", "background-size", "parse_background_size",
                "apply_background_size", PropertyFlags::LayoutAffecting)

// Alias property names that map to canonical entries
HB_CSS_PROPERTY_ALIAS(BoxSizing, WebkitBoxSizing, "-webkit-box-sizing", "box-sizing", "parse_identifier",
                      "apply_box_sizing", PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY_ALIAS(BoxSizing, MozBoxSizing, "-moz-box-sizing", "box-sizing", "parse_identifier",
                      "apply_box_sizing", PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY_ALIAS(BoxSizing, MsBoxSizing, "-ms-box-sizing", "box-sizing", "parse_identifier", "apply_box_sizing",
                      PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY_ALIAS(BoxSizing, OBoxSizing, "-o-box-sizing", "box-sizing", "parse_identifier", "apply_box_sizing",
                      PropertyFlags::LayoutAffecting)
