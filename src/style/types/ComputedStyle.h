#pragma once

#include <optional>
#include <string>
#include <unordered_map>

#include "core/platform_api/IGraphicsContext.h"

namespace Hummingbird::Css {

struct EdgeSizes {
    float top = 0;
    float right = 0;
    float bottom = 0;
    float left = 0;
};

struct ComputedStyle {
    enum class Display { Block, Inline, InlineBlock, ListItem, Flex, None };
    Display display = Display::Block;
    enum class FlexDirection { Row, RowReverse, Column, ColumnReverse };
    FlexDirection flex_direction = FlexDirection::Row;
    enum class FlexWrap { NoWrap, Wrap, WrapReverse };
    FlexWrap flex_wrap = FlexWrap::NoWrap;
    enum class JustifyContent { FlexStart, FlexEnd, Center, SpaceBetween, SpaceAround, SpaceEvenly };
    JustifyContent justify_content = JustifyContent::FlexStart;
    enum class AlignItems { Stretch, FlexStart, FlexEnd, Center, Baseline };
    AlignItems align_items = AlignItems::Stretch;
    float flex_grow = 0.0f;
    float flex_shrink = 1.0f;
    std::optional<float> flex_basis;
    bool flex_basis_is_percent = false;
    int order = 0;
    enum class Float { None, Left, Right };
    Float float_type = Float::None;
    enum class Position { Static, Relative, Absolute };
    Position position = Position::Static;
    enum class TextAlign { Left, Center, Right };
    enum class Cursor { Auto, Default, Pointer, Text };
    enum class VerticalAlign { Baseline, Top, Middle, Bottom };
    enum class Overflow { Visible, Hidden, Scroll, Auto };
    Overflow overflow_x = Overflow::Visible;
    Overflow overflow_y = Overflow::Visible;
    Cursor cursor = Cursor::Auto;
    VerticalAlign vertical_align = VerticalAlign::Baseline;
    TextAlign text_align = TextAlign::Left;
    enum class TextTransform { None, Uppercase, Lowercase, Capitalize };
    TextTransform text_transform = TextTransform::None;
    float letter_spacing = 0.0f;
    float text_indent = 0.0f;
    enum class TextOverflow { Clip, Ellipsis };
    TextOverflow text_overflow = TextOverflow::Clip;
    enum class WordWrap { Normal, BreakWord };
    WordWrap word_wrap = WordWrap::Normal;
    enum class BorderStyle { None, Solid, Outset, Inset, Ridge, Groove };
    BorderStyle border_style = BorderStyle::None;
    EdgeSizes border_width;
    float border_radius = 0.0f;
    Color border_color{0, 0, 0, 255};
    float outline_width = 0.0f;
    float outline_offset = 0.0f;
    Color outline_color{0, 0, 0, 255};
    EdgeSizes margin;
    bool margin_left_auto = false;
    bool margin_right_auto = false;
    EdgeSizes padding;
    enum class BoxSizing { ContentBox, BorderBox };
    BoxSizing box_sizing = BoxSizing::ContentBox;
    bool transform_has_translate = false;
    float transform_translate_x = 0.0f;
    float transform_translate_y = 0.0f;
    std::optional<float> width;
    std::optional<float> height;
    std::optional<float> min_width;
    std::optional<float> min_height;
    std::optional<float> max_width;
    std::optional<float> max_height;
    bool width_is_percent = false;
    bool height_is_percent = false;
    bool min_width_is_percent = false;
    bool min_height_is_percent = false;
    bool max_width_is_percent = false;
    bool max_height_is_percent = false;
    std::optional<float> top;
    std::optional<float> right;
    std::optional<float> bottom;
    std::optional<float> left;
    bool top_is_percent = false;
    bool right_is_percent = false;
    bool bottom_is_percent = false;
    bool left_is_percent = false;
    std::optional<int> z_index;
    float opacity = 1.0f;
    Color color{0, 0, 0, 255};
    bool underline = false;
    std::optional<float> underline_thickness;
    std::optional<float> underline_offset;
    std::optional<Color> link_color;
    std::optional<Color> vlink_color;
    bool font_monospace = false;
    enum class WhiteSpace { Normal, Preserve, NoWrap };
    WhiteSpace whitespace = WhiteSpace::Normal;
    enum class FontWeight { Normal, Bold };
    enum class FontStyle { Normal, Italic };
    FontWeight weight = FontWeight::Normal;
    FontStyle style = FontStyle::Normal;
    float font_size = 16.0f;   // px
    float line_height = 0.0f;  // px, 0 means use font metrics
    std::string font_face;
    std::optional<Color> background;
    std::optional<std::string> background_image;
    struct BoxShadow {
        float offset_x = 0.0f;
        float offset_y = 0.0f;
        float blur = 0.0f;
        Color color{0, 0, 0, 255};
    };
    std::optional<BoxShadow> box_shadow;
    enum class BackgroundRepeat { Repeat, NoRepeat, RepeatX, RepeatY };
    BackgroundRepeat background_repeat = BackgroundRepeat::Repeat;
    struct BackgroundPosition {
        enum class Horizontal { Left, Center, Right };
        enum class Vertical { Top, Center, Bottom };
        Horizontal horizontal = Horizontal::Left;
        Vertical vertical = Vertical::Top;
        std::optional<float> offset_x;
        std::optional<float> offset_y;
    };
    BackgroundPosition background_position;
    struct BackgroundSize {
        enum class Type { Auto, Contain, Cover, Length };
        Type type = Type::Auto;
        std::optional<float> width;
        std::optional<float> height;
    };
    BackgroundSize background_size;
    enum class ListStyleType { Disc, Decimal, None };
    enum class ListStylePosition { Outside, Inside };
    ListStyleType list_style_type = ListStyleType::Disc;
    ListStylePosition list_style_position = ListStylePosition::Outside;
    std::unordered_map<std::string, std::string> custom_properties;
};

inline ComputedStyle default_computed_style() {
    return ComputedStyle{};
}

}  // namespace Hummingbird::Css
