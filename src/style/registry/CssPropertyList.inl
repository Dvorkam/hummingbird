// Canonical property entries
HB_CSS_PROPERTY(Display, Display, "display", "display", ParserHook::parse_identifier, ApplyHook::apply_display,
                PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(Position, Position, "position", "position", ParserHook::parse_identifier, ApplyHook::apply_position,
                PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(FlexDirection, FlexDirection, "flex-direction", "flex-direction", ParserHook::parse_identifier,
                ApplyHook::apply_flex_direction, PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(FlexWrap, FlexWrap, "flex-wrap", "flex-wrap", ParserHook::parse_identifier, ApplyHook::apply_flex_wrap,
                PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(JustifyContent, JustifyContent, "justify-content", "justify-content", ParserHook::parse_identifier,
                ApplyHook::apply_justify_content, PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(AlignItems, AlignItems, "align-items", "align-items", ParserHook::parse_identifier,
                ApplyHook::apply_align_items, PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(FlexGrow, FlexGrow, "flex-grow", "flex-grow", ParserHook::parse_length_number,
                ApplyHook::apply_flex_grow, PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(FlexShrink, FlexShrink, "flex-shrink", "flex-shrink", ParserHook::parse_length_number,
                ApplyHook::apply_flex_shrink, PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(FlexBasis, FlexBasis, "flex-basis", "flex-basis", ParserHook::parse_length_auto,
                ApplyHook::apply_flex_basis, PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(Flex, Flex, "flex", "flex", ParserHook::parse_flex_shorthand, ApplyHook::apply_flex_grow,
                PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(Order, Order, "order", "order", ParserHook::parse_length_number, ApplyHook::apply_order,
                PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(GridTemplateColumns, GridTemplateColumns, "grid-template-columns", "grid-template-columns",
                ParserHook::parse_grid_track_list, ApplyHook::apply_grid, PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(GridTemplateRows, GridTemplateRows, "grid-template-rows", "grid-template-rows",
                ParserHook::parse_grid_track_list, ApplyHook::apply_grid, PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(GridAutoRows, GridAutoRows, "grid-auto-rows", "grid-auto-rows", ParserHook::parse_grid_track_list,
                ApplyHook::apply_grid, PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(GridColumn, GridColumn, "grid-column", "grid-column", ParserHook::parse_grid_placement,
                ApplyHook::apply_grid, PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(GridRow, GridRow, "grid-row", "grid-row", ParserHook::parse_grid_placement, ApplyHook::apply_grid,
                PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(Gap, Gap, "gap", "gap", ParserHook::parse_gap, ApplyHook::apply_grid, PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY_ALIAS(Gap, GridGap, "grid-gap", "gap", ParserHook::parse_gap, ApplyHook::apply_grid,
                      PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(RowGap, RowGap, "row-gap", "row-gap", ParserHook::parse_length, ApplyHook::apply_grid,
                PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY_ALIAS(RowGap, GridRowGap, "grid-row-gap", "row-gap", ParserHook::parse_length, ApplyHook::apply_grid,
                      PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(ColumnGap, ColumnGap, "column-gap", "column-gap", ParserHook::parse_length, ApplyHook::apply_grid,
                PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY_ALIAS(ColumnGap, GridColumnGap, "grid-column-gap", "column-gap", ParserHook::parse_length,
                      ApplyHook::apply_grid, PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(Overflow, Overflow, "overflow", "overflow", ParserHook::parse_identifier, ApplyHook::apply_overflow,
                PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(OverflowY, OverflowY, "overflow-y", "overflow-y", ParserHook::parse_identifier,
                ApplyHook::apply_overflow, PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(Font, Font, "font", "font", ParserHook::parse_font_shorthand, ApplyHook::apply_font_family,
                PropertyFlags::Inherited | PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(FontSize, FontSize, "font-size", "font-size", ParserHook::parse_font_size, ApplyHook::apply_font_size,
                PropertyFlags::Inherited | PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(LineHeight, LineHeight, "line-height", "line-height", ParserHook::parse_length_number,
                ApplyHook::apply_line_height, PropertyFlags::Inherited | PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(Margin, Margin, "margin", "margin", ParserHook::parse_margin_shorthand, ApplyHook::apply_margin,
                PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(MarginTop, MarginTop, "margin-top", "margin-top", ParserHook::parse_length_auto,
                ApplyHook::apply_margin_top, PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(MarginRight, MarginRight, "margin-right", "margin-right", ParserHook::parse_length_auto,
                ApplyHook::apply_margin_right, PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(MarginBottom, MarginBottom, "margin-bottom", "margin-bottom", ParserHook::parse_length_auto,
                ApplyHook::apply_margin_bottom, PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(MarginLeft, MarginLeft, "margin-left", "margin-left", ParserHook::parse_length_auto,
                ApplyHook::apply_margin_left, PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(Padding, Padding, "padding", "padding", ParserHook::parse_padding_shorthand, ApplyHook::apply_padding,
                PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(PaddingTop, PaddingTop, "padding-top", "padding-top", ParserHook::parse_length,
                ApplyHook::apply_padding_top, PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(PaddingRight, PaddingRight, "padding-right", "padding-right", ParserHook::parse_length,
                ApplyHook::apply_padding_right, PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(PaddingBottom, PaddingBottom, "padding-bottom", "padding-bottom", ParserHook::parse_length,
                ApplyHook::apply_padding_bottom, PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(PaddingLeft, PaddingLeft, "padding-left", "padding-left", ParserHook::parse_length,
                ApplyHook::apply_padding_left, PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(BoxSizing, BoxSizing, "box-sizing", "box-sizing", ParserHook::parse_identifier,
                ApplyHook::apply_box_sizing, PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(Transform, Transform, "transform", "transform", ParserHook::parse_transform, ApplyHook::apply_transform,
                PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(Border, Border, "border", "border", ParserHook::parse_border_shorthand, ApplyHook::apply_border,
                PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(BorderTop, BorderTop, "border-top", "border-top", ParserHook::parse_border_shorthand,
                ApplyHook::apply_border, PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(BorderRight, BorderRight, "border-right", "border-right", ParserHook::parse_border_shorthand,
                ApplyHook::apply_border, PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(BorderBottom, BorderBottom, "border-bottom", "border-bottom", ParserHook::parse_border_shorthand,
                ApplyHook::apply_border, PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(BorderLeft, BorderLeft, "border-left", "border-left", ParserHook::parse_border_shorthand,
                ApplyHook::apply_border, PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(BorderWidth, BorderWidth, "border-width", "border-width", ParserHook::parse_length,
                ApplyHook::apply_border_width, PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(BorderTopWidth, BorderTopWidth, "border-top-width", "border-top-width", ParserHook::parse_length,
                ApplyHook::apply_border_width, PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(BorderRightWidth, BorderRightWidth, "border-right-width", "border-right-width",
                ParserHook::parse_length, ApplyHook::apply_border_width, PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(BorderBottomWidth, BorderBottomWidth, "border-bottom-width", "border-bottom-width",
                ParserHook::parse_length, ApplyHook::apply_border_width, PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(BorderLeftWidth, BorderLeftWidth, "border-left-width", "border-left-width", ParserHook::parse_length,
                ApplyHook::apply_border_width, PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(BorderRadius, BorderRadius, "border-radius", "border-radius", ParserHook::parse_border_radius,
                ApplyHook::apply_border_radius, PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(BorderTopLeftRadius, BorderTopLeftRadius, "border-top-left-radius", "border-top-left-radius",
                ParserHook::parse_length, ApplyHook::apply_border_radius, PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(BorderTopRightRadius, BorderTopRightRadius, "border-top-right-radius", "border-top-right-radius",
                ParserHook::parse_length, ApplyHook::apply_border_radius, PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(BorderBottomRightRadius, BorderBottomRightRadius, "border-bottom-right-radius",
                "border-bottom-right-radius", ParserHook::parse_length, ApplyHook::apply_border_radius,
                PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(BorderBottomLeftRadius, BorderBottomLeftRadius, "border-bottom-left-radius",
                "border-bottom-left-radius", ParserHook::parse_length, ApplyHook::apply_border_radius,
                PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(BorderColor, BorderColor, "border-color", "border-color", ParserHook::parse_color,
                ApplyHook::apply_border_color, PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(BorderTopColor, BorderTopColor, "border-top-color", "border-top-color", ParserHook::parse_color,
                ApplyHook::apply_border_color, PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(BorderRightColor, BorderRightColor, "border-right-color", "border-right-color", ParserHook::parse_color,
                ApplyHook::apply_border_color, PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(BorderBottomColor, BorderBottomColor, "border-bottom-color", "border-bottom-color",
                ParserHook::parse_color, ApplyHook::apply_border_color, PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(BorderLeftColor, BorderLeftColor, "border-left-color", "border-left-color", ParserHook::parse_color,
                ApplyHook::apply_border_color, PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(Outline, Outline, "outline", "outline", ParserHook::parse_outline_shorthand, ApplyHook::apply_outline,
                PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(OutlineWidth, OutlineWidth, "outline-width", "outline-width", ParserHook::parse_length,
                ApplyHook::apply_outline_width, PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(OutlineColor, OutlineColor, "outline-color", "outline-color", ParserHook::parse_color,
                ApplyHook::apply_outline_color, PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(OutlineOffset, OutlineOffset, "outline-offset", "outline-offset", ParserHook::parse_length,
                ApplyHook::apply_outline_offset, PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(BorderStyle, BorderStyle, "border-style", "border-style", ParserHook::parse_identifier,
                ApplyHook::apply_border_style, PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(Width, Width, "width", "width", ParserHook::parse_length_auto, ApplyHook::apply_width,
                PropertyFlags::LayoutAffecting)
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
HB_CSS_PROPERTY(Top, Top, "top", "top", ParserHook::parse_length_auto, ApplyHook::apply_top,
                PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(Right, Right, "right", "right", ParserHook::parse_length_auto, ApplyHook::apply_right,
                PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(Bottom, Bottom, "bottom", "bottom", ParserHook::parse_length_auto, ApplyHook::apply_bottom,
                PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(Left, Left, "left", "left", ParserHook::parse_length_auto, ApplyHook::apply_left,
                PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(ZIndex, ZIndex, "z-index", "z-index", ParserHook::parse_number_auto, ApplyHook::apply_z_index,
                PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(Opacity, Opacity, "opacity", "opacity", ParserHook::parse_opacity, ApplyHook::apply_opacity,
                PropertyFlags::None)
HB_CSS_PROPERTY(TextAlign, TextAlign, "text-align", "text-align", ParserHook::parse_identifier,
                ApplyHook::apply_text_align, PropertyFlags::Inherited | PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(TextTransform, TextTransform, "text-transform", "text-transform", ParserHook::parse_identifier,
                ApplyHook::apply_text_transform, PropertyFlags::Inherited | PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(Cursor, Cursor, "cursor", "cursor", ParserHook::parse_identifier, ApplyHook::apply_cursor,
                PropertyFlags::Inherited)
HB_CSS_PROPERTY(Visibility, Visibility, "visibility", "visibility", ParserHook::parse_identifier,
                ApplyHook::apply_visibility, PropertyFlags::Inherited)
HB_CSS_PROPERTY(PointerEvents, PointerEvents, "pointer-events", "pointer-events", ParserHook::parse_identifier,
                ApplyHook::apply_pointer_events, PropertyFlags::Inherited)
HB_CSS_PROPERTY(VerticalAlign, VerticalAlign, "vertical-align", "vertical-align", ParserHook::parse_identifier,
                ApplyHook::apply_vertical_align, PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(LetterSpacing, LetterSpacing, "letter-spacing", "letter-spacing", ParserHook::parse_length_number,
                ApplyHook::apply_letter_spacing, PropertyFlags::Inherited | PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(TextIndent, TextIndent, "text-indent", "text-indent", ParserHook::parse_length,
                ApplyHook::apply_text_indent, PropertyFlags::Inherited | PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(TextOverflow, TextOverflow, "text-overflow", "text-overflow", ParserHook::parse_identifier,
                ApplyHook::apply_text_overflow, PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(Clip, Clip, "clip", "clip", ParserHook::parse_clip, ApplyHook::apply_clip,
                PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(WordWrap, WordWrap, "word-wrap", "word-wrap", ParserHook::parse_identifier, ApplyHook::apply_word_wrap,
                PropertyFlags::Inherited | PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(TextDecoration, TextDecoration, "text-decoration", "text-decoration", ParserHook::parse_text_decoration,
                ApplyHook::apply_text_decoration, PropertyFlags::Inherited)
HB_CSS_PROPERTY(TextDecorationThickness, TextDecorationThickness, "text-decoration-thickness",
                "text-decoration-thickness", ParserHook::parse_length_number,
                ApplyHook::apply_text_decoration_thickness, PropertyFlags::Inherited)
HB_CSS_PROPERTY(TextUnderlineOffset, TextUnderlineOffset, "text-underline-offset", "text-underline-offset",
                ParserHook::parse_length_number, ApplyHook::apply_text_underline_offset, PropertyFlags::Inherited)
HB_CSS_PROPERTY(WhiteSpace, WhiteSpace, "white-space", "white-space", ParserHook::parse_identifier,
                ApplyHook::apply_white_space, PropertyFlags::Inherited | PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(FontFamily, FontFamily, "font-family", "font-family", ParserHook::parse_font_family,
                ApplyHook::apply_font_family, PropertyFlags::Inherited | PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(FontWeight, FontWeight, "font-weight", "font-weight", ParserHook::parse_font_weight,
                ApplyHook::apply_font_weight, PropertyFlags::Inherited | PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(FontStyle, FontStyle, "font-style", "font-style", ParserHook::parse_identifier,
                ApplyHook::apply_font_style, PropertyFlags::Inherited | PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(Float, Float, "float", "float", ParserHook::parse_identifier, ApplyHook::apply_float,
                PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(Clear, Clear, "clear", "clear", ParserHook::parse_identifier, ApplyHook::apply_clear,
                PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(ListStyle, ListStyle, "list-style", "list-style", ParserHook::parse_list_style_shorthand,
                ApplyHook::apply_list_style, PropertyFlags::Inherited | PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(ListStyleType, ListStyleType, "list-style-type", "list-style-type", ParserHook::parse_identifier,
                ApplyHook::apply_list_style_type, PropertyFlags::Inherited | PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(ListStylePosition, ListStylePosition, "list-style-position", "list-style-position",
                ParserHook::parse_identifier, ApplyHook::apply_list_style_position,
                PropertyFlags::Inherited | PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(Color, Color, "color", "color", ParserHook::parse_color, ApplyHook::apply_color,
                PropertyFlags::Inherited | PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(BackgroundColor, BackgroundColor, "background-color", "background-color", ParserHook::parse_color,
                ApplyHook::apply_background_color, PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(Background, Background, "background", "background", ParserHook::parse_background_shorthand,
                ApplyHook::apply_background, PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(BackgroundImage, BackgroundImage, "background-image", "background-image",
                ParserHook::parse_background_image, ApplyHook::apply_background_image, PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(BackgroundRepeat, BackgroundRepeat, "background-repeat", "background-repeat",
                ParserHook::parse_background_repeat, ApplyHook::apply_background_repeat, PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(BackgroundPosition, BackgroundPosition, "background-position", "background-position",
                ParserHook::parse_background_position, ApplyHook::apply_background_position,
                PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(BackgroundSize, BackgroundSize, "background-size", "background-size", ParserHook::parse_background_size,
                ApplyHook::apply_background_size, PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY(BoxShadow, BoxShadow, "box-shadow", "box-shadow", ParserHook::parse_box_shadow,
                ApplyHook::apply_box_shadow, PropertyFlags::None)
// Animation properties (T-ANIM-1): recognized but applied statically, i.e. as
// no-ops — with no timing engine the property simply takes its target value and
// the transition/timing metadata has no visual effect. Registering them keeps
// them out of the "unsupported property" warning stream.
HB_CSS_PROPERTY(Transition, Transition, "transition", "transition", ParserHook::parse_passthrough,
                ApplyHook::apply_noop, PropertyFlags::None)
HB_CSS_PROPERTY(TransitionProperty, TransitionProperty, "transition-property", "transition-property",
                ParserHook::parse_passthrough, ApplyHook::apply_noop, PropertyFlags::None)
HB_CSS_PROPERTY(TransitionDuration, TransitionDuration, "transition-duration", "transition-duration",
                ParserHook::parse_passthrough, ApplyHook::apply_noop, PropertyFlags::None)
HB_CSS_PROPERTY(TransitionDelay, TransitionDelay, "transition-delay", "transition-delay", ParserHook::parse_passthrough,
                ApplyHook::apply_noop, PropertyFlags::None)
HB_CSS_PROPERTY(TransitionTimingFunction, TransitionTimingFunction, "transition-timing-function",
                "transition-timing-function", ParserHook::parse_passthrough, ApplyHook::apply_noop, PropertyFlags::None)
HB_CSS_PROPERTY(TransformOrigin, TransformOrigin, "transform-origin", "transform-origin", ParserHook::parse_passthrough,
                ApplyHook::apply_noop, PropertyFlags::None)
HB_CSS_PROPERTY(TextShadow, TextShadow, "text-shadow", "text-shadow", ParserHook::parse_box_shadow,
                ApplyHook::apply_text_shadow, PropertyFlags::Inherited)

// Alias property names that map to canonical entries
HB_CSS_PROPERTY_ALIAS(BoxSizing, WebkitBoxSizing, "-webkit-box-sizing", "box-sizing", ParserHook::parse_identifier,
                      ApplyHook::apply_box_sizing, PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY_ALIAS(BoxSizing, MozBoxSizing, "-moz-box-sizing", "box-sizing", ParserHook::parse_identifier,
                      ApplyHook::apply_box_sizing, PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY_ALIAS(BoxSizing, MsBoxSizing, "-ms-box-sizing", "box-sizing", ParserHook::parse_identifier,
                      ApplyHook::apply_box_sizing, PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY_ALIAS(BoxSizing, OBoxSizing, "-o-box-sizing", "box-sizing", ParserHook::parse_identifier,
                      ApplyHook::apply_box_sizing, PropertyFlags::LayoutAffecting)

// Vendor-prefixed border-radius shorthand (all resolve to the standard property).
HB_CSS_PROPERTY_ALIAS(BorderRadius, WebkitBorderRadius, "-webkit-border-radius", "border-radius",
                      ParserHook::parse_border_radius, ApplyHook::apply_border_radius, PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY_ALIAS(BorderRadius, MozBorderRadius, "-moz-border-radius", "border-radius",
                      ParserHook::parse_border_radius, ApplyHook::apply_border_radius, PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY_ALIAS(BorderRadius, MsBorderRadius, "-ms-border-radius", "border-radius",
                      ParserHook::parse_border_radius, ApplyHook::apply_border_radius, PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY_ALIAS(BorderRadius, OBorderRadius, "-o-border-radius", "border-radius", ParserHook::parse_border_radius,
                      ApplyHook::apply_border_radius, PropertyFlags::LayoutAffecting)

// Vendor-prefixed per-corner radius. WebKit uses the standard corner names;
// Gecko uses the older `-moz-border-radius-<corner>` spelling.
HB_CSS_PROPERTY_ALIAS(BorderTopLeftRadius, WebkitBorderTopLeftRadius, "-webkit-border-top-left-radius",
                      "border-top-left-radius", ParserHook::parse_length, ApplyHook::apply_border_radius,
                      PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY_ALIAS(BorderTopRightRadius, WebkitBorderTopRightRadius, "-webkit-border-top-right-radius",
                      "border-top-right-radius", ParserHook::parse_length, ApplyHook::apply_border_radius,
                      PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY_ALIAS(BorderBottomRightRadius, WebkitBorderBottomRightRadius, "-webkit-border-bottom-right-radius",
                      "border-bottom-right-radius", ParserHook::parse_length, ApplyHook::apply_border_radius,
                      PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY_ALIAS(BorderBottomLeftRadius, WebkitBorderBottomLeftRadius, "-webkit-border-bottom-left-radius",
                      "border-bottom-left-radius", ParserHook::parse_length, ApplyHook::apply_border_radius,
                      PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY_ALIAS(BorderTopLeftRadius, MozBorderRadiusTopleft, "-moz-border-radius-topleft",
                      "border-top-left-radius", ParserHook::parse_length, ApplyHook::apply_border_radius,
                      PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY_ALIAS(BorderTopRightRadius, MozBorderRadiusTopright, "-moz-border-radius-topright",
                      "border-top-right-radius", ParserHook::parse_length, ApplyHook::apply_border_radius,
                      PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY_ALIAS(BorderBottomRightRadius, MozBorderRadiusBottomright, "-moz-border-radius-bottomright",
                      "border-bottom-right-radius", ParserHook::parse_length, ApplyHook::apply_border_radius,
                      PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY_ALIAS(BorderBottomLeftRadius, MozBorderRadiusBottomleft, "-moz-border-radius-bottomleft",
                      "border-bottom-left-radius", ParserHook::parse_length, ApplyHook::apply_border_radius,
                      PropertyFlags::LayoutAffecting)

// Vendor-prefixed text-overflow: Opera and IE shipped this before it was
// unprefixed. Behavior-equivalent, so alias to the standard property.
HB_CSS_PROPERTY_ALIAS(TextOverflow, OTextOverflow, "-o-text-overflow", "text-overflow", ParserHook::parse_identifier,
                      ApplyHook::apply_text_overflow, PropertyFlags::LayoutAffecting)
HB_CSS_PROPERTY_ALIAS(TextOverflow, MsTextOverflow, "-ms-text-overflow", "text-overflow", ParserHook::parse_identifier,
                      ApplyHook::apply_text_overflow, PropertyFlags::LayoutAffecting)
