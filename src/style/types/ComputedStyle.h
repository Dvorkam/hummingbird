#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/platform_api/IGraphicsContext.h"

namespace Hummingbird::Css {

struct EdgeSizes {
    float top = 0;
    float right = 0;
    float bottom = 0;
    float left = 0;
};

// Per-side border colors. `border`/`border-color` set all four; the
// `border-<side>-color` longhands set one. Painting uses `top` as the
// representative color for uniform (rounded) borders and each side
// individually for the square multi-edge path.
struct EdgeColors {
    Color top{0, 0, 0, 255};
    Color right{0, 0, 0, 255};
    Color bottom{0, 0, 0, 255};
    Color left{0, 0, 0, 255};
};

// One corner's radius, either an absolute px value or a percentage of the
// box's reference length (resolved at paint time — percentages need the box).
struct CornerRadius {
    float value = 0.0f;
    bool percent = false;
    float resolve(float reference) const { return percent ? value * 0.01f * reference : value; }
};

struct CornerRadii {
    CornerRadius top_left;
    CornerRadius top_right;
    CornerRadius bottom_right;
    CornerRadius bottom_left;

    void set_all(CornerRadius corner) { top_left = top_right = bottom_right = bottom_left = corner; }
    bool any() const {
        return top_left.value > 0.0f || top_right.value > 0.0f || bottom_right.value > 0.0f || bottom_left.value > 0.0f;
    }
    bool uniform() const {
        auto same = [](const CornerRadius& a, const CornerRadius& b) {
            return a.value == b.value && a.percent == b.percent;
        };
        return same(top_left, top_right) && same(top_left, bottom_right) && same(top_left, bottom_left);
    }
};

struct ComputedStyle {
    // A computed length that may carry both an absolute part (px, with em already
    // resolved at apply time) and a percentage part, so `calc(100% - 20px)` is one
    // value. Plain lengths use only `px`; plain percentages only `percent`. The
    // percentage resolves against a reference length supplied at layout time.
    struct LengthValue {
        float px = 0.0f;
        float percent = 0.0f;
        bool has_percent = false;

        static LengthValue from_px(float value) { return {value, 0.0f, false}; }
        static LengthValue from_percent(float value) { return {0.0f, value, true}; }

        float resolve(float reference) const { return px + (has_percent ? percent * 0.01f * reference : 0.0f); }
    };

    enum class Display { Block, Inline, InlineBlock, ListItem, Flex, Grid, None };
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
    std::optional<LengthValue> flex_basis;
    int order = 0;

    // --- CSS Grid (display: grid). Fixed/fr/%/auto tracks, gaps, row-major
    // auto-placement, and line/span item placement (T-LAYOUT-GRID-1). ---
    struct GridTrack {
        enum class Kind { Fixed, Fr, Percent, Auto };
        Kind kind = Kind::Auto;
        float value = 0.0f;  // px (Fixed), fraction (Fr), percent (Percent); ignored for Auto

        static GridTrack fixed(float px) { return {Kind::Fixed, px}; }
        static GridTrack fr(float f) { return {Kind::Fr, f}; }
        static GridTrack percent(float p) { return {Kind::Percent, p}; }
        static GridTrack automatic() { return {Kind::Auto, 0.0f}; }
    };
    // Item placement on one axis. `line == 0` means auto-place; `line >= 1` is a
    // 1-based explicit grid line; `span` is the track count (>= 1).
    struct GridPlacement {
        int line = 0;
        int span = 1;
    };

    std::vector<GridTrack> grid_template_columns;
    std::vector<GridTrack> grid_template_rows;
    GridTrack grid_auto_rows{GridTrack::Kind::Auto, 0.0f};  // implicit-row sizing
    float row_gap = 0.0f;
    float column_gap = 0.0f;
    GridPlacement grid_column;
    GridPlacement grid_row;
    enum class Float { None, Left, Right };
    Float float_type = Float::None;
    enum class Clear { None, Left, Right, Both };
    Clear clear = Clear::None;
    enum class Position { Static, Relative, Absolute };
    Position position = Position::Static;
    // Inherited. `hidden`/`collapse` keep layout space but suppress paint and
    // hit-testing; a descendant may reveal itself again with `visibility:visible`.
    enum class Visibility { Visible, Hidden, Collapse };
    Visibility visibility = Visibility::Visible;
    // Inherited. `none` makes the box transparent to hit-testing (pointer events
    // fall through to what is behind it); a descendant may opt back in with `auto`.
    enum class PointerEvents { Auto, None };
    PointerEvents pointer_events = PointerEvents::Auto;
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
    CornerRadii border_radius;
    Color border_color{0, 0, 0, 255};
    EdgeColors border_edge_color;
    float outline_width = 0.0f;
    float outline_offset = 0.0f;
    Color outline_color{0, 0, 0, 255};
    EdgeSizes margin;
    bool margin_left_auto = false;
    bool margin_right_auto = false;
    bool margin_top_auto = false;
    bool margin_bottom_auto = false;
    EdgeSizes padding;
    enum class BoxSizing { ContentBox, BorderBox };
    BoxSizing box_sizing = BoxSizing::ContentBox;
    bool transform_has_translate = false;
    float transform_translate_x = 0.0f;
    float transform_translate_y = 0.0f;
    std::optional<LengthValue> width;
    std::optional<LengthValue> height;
    std::optional<LengthValue> min_width;
    std::optional<LengthValue> min_height;
    std::optional<LengthValue> max_width;
    std::optional<LengthValue> max_height;
    std::optional<LengthValue> top;
    std::optional<LengthValue> right;
    std::optional<LengthValue> bottom;
    std::optional<LengthValue> left;
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
    // Resolved @font-face key for the active family (a loadable asset path or a
    // remote font url whose bytes are registered with the font cache). Empty
    // means "no web font matched"; fall back to the built-in family mapping.
    // Derived from font_face after cascade + inheritance, so it is not itself an
    // independently inherited/overridable property.
    std::string font_src;
    std::optional<Color> background;
    std::optional<std::string> background_image;
    struct BoxShadow {
        float offset_x = 0.0f;
        float offset_y = 0.0f;
        float blur = 0.0f;
        Color color{0, 0, 0, 255};
    };
    std::optional<BoxShadow> box_shadow;
    // Inherited. Same value shape as box-shadow (offset + blur + color), no
    // spread/inset. Painted as an offset copy of the glyphs behind the text.
    std::optional<BoxShadow> text_shadow;
    enum class BackgroundRepeat { Repeat, NoRepeat, RepeatX, RepeatY };
    BackgroundRepeat background_repeat = BackgroundRepeat::Repeat;
    struct BackgroundPosition {
        enum class Horizontal { Left, Center, Right };
        enum class Vertical { Top, Center, Bottom };
        Horizontal horizontal = Horizontal::Left;
        Vertical vertical = Vertical::Top;
        std::optional<float> offset_x;
        std::optional<float> offset_y;
        // Percentages position by (box - image) * pct: 0%=start, 50%=center,
        // 100%=end (resolved at paint time). Otherwise the offset is px.
        bool offset_x_is_percent = false;
        bool offset_y_is_percent = false;
    };
    BackgroundPosition background_position;
    struct BackgroundSize {
        enum class Type { Auto, Contain, Cover, Length };
        Type type = Type::Auto;
        std::optional<float> width;   // nullopt => auto (aspect-preserved)
        std::optional<float> height;  // nullopt => auto (aspect-preserved)
        bool width_is_percent = false;
        bool height_is_percent = false;
    };
    BackgroundSize background_size;
    // Legacy `clip: rect(...)`, applies only to absolutely-positioned boxes.
    // Edges are px offsets from the box's top-left; nullopt means `auto` (the
    // box edge). The dominant real-world use is the accessibility "visually
    // hidden" pattern (`clip:rect(0 0 0 0)`), a degenerate rect that hides the
    // element; hides_content() detects it. Non-degenerate rectangular clipping
    // is stored but not yet applied at paint time.
    struct ClipRect {
        std::optional<float> top;
        std::optional<float> right;
        std::optional<float> bottom;
        std::optional<float> left;
        bool hides_content() const {
            const bool empty_x = left && right && *right <= *left;
            const bool empty_y = top && bottom && *bottom <= *top;
            return empty_x || empty_y;
        }
    };
    std::optional<ClipRect> clip;
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
