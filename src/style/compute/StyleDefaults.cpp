#include "style/compute/StyleDefaults.h"

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "core/GraphicsTypes.h"
#include "core/dom/Element.h"
#include "core/utils/ColorUtils.h"
#include "core/utils/ParseUtils.h"
#include "core/utils/StringUtils.h"
#include "html/HtmlAttributeNames.h"
#include "html/HtmlTagNames.h"
#include "style/types/ComputedStyle.h"

namespace Hummingbird::Css::StyleDefaults {

namespace {
constexpr float kTextareaDefaultWidth = 360.0f;
constexpr float kTextareaDefaultVerticalPadding = 4.0f;
constexpr float kTextareaLineHeightFactor = 1.2f;
constexpr long kTextareaDefaultRows = 2;
// `rows` is author input: cap it so a typo cannot demand a pathological box.
constexpr long kTextareaMaxRows = 100;

bool input_type_is_text_like(const DOM::Element& element) {
    const auto* type = element.find_attribute(Hummingbird::Html::AttributeNames::Type);
    if (!type || type->empty()) {
        return true;
    }
    return !Core::Utils::equals_ignore_case(*type, "button") && !Core::Utils::equals_ignore_case(*type, "submit") &&
           !Core::Utils::equals_ignore_case(*type, "reset") && !Core::Utils::equals_ignore_case(*type, "checkbox") &&
           !Core::Utils::equals_ignore_case(*type, "radio") && !Core::Utils::equals_ignore_case(*type, "file") &&
           !Core::Utils::equals_ignore_case(*type, "hidden") && !Core::Utils::equals_ignore_case(*type, "image");
}

long textarea_rows(const DOM::Element& element) {
    const auto* rows_attribute = element.find_attribute(Hummingbird::Html::AttributeNames::Rows);
    if (!rows_attribute) {
        return kTextareaDefaultRows;
    }
    auto parsed = Core::Utils::parse_long(*rows_attribute, Core::Utils::NumberParseMode::Strict);
    if (!parsed) {
        return kTextareaDefaultRows;
    }
    return std::clamp(*parsed, 1L, kTextareaMaxRows);
}

bool input_type_is_toggle(const DOM::Element& element) {
    const auto* type = element.find_attribute(Hummingbird::Html::AttributeNames::Type);
    return type &&
           (Core::Utils::equals_ignore_case(*type, "checkbox") || Core::Utils::equals_ignore_case(*type, "radio"));
}

bool input_type_is_hidden(const DOM::Element& element) {
    const auto* type = element.find_attribute(Hummingbird::Html::AttributeNames::Type);
    return type && Core::Utils::equals_ignore_case(*type, "hidden");
}
}  // namespace

void apply_user_agent_defaults(const DOM::Element& element, ComputedStyle& style, StyleOverrides& overrides,
                               bool display_set, const ComputedStyle* parent_style) {
    const auto& tag = element.get_tag_name();
    const auto set_heading = [&](float scale, float margin_em) {
        style.font_size = 16.0f * scale;
        style.weight = ComputedStyle::FontWeight::Bold;
        float m = style.font_size * margin_em;
        style.margin.top = style.margin.bottom = m;
    };

    if (!display_set) {
        if (tag == Hummingbird::Html::TagNames::A || tag == Hummingbird::Html::TagNames::Span ||
            tag == Hummingbird::Html::TagNames::Strong || tag == Hummingbird::Html::TagNames::Em ||
            tag == Hummingbird::Html::TagNames::B || tag == Hummingbird::Html::TagNames::I ||
            tag == Hummingbird::Html::TagNames::Code || tag == Hummingbird::Html::TagNames::Img ||
            tag == Hummingbird::Html::TagNames::Svg || tag == Hummingbird::Html::TagNames::Font) {
            style.display = ComputedStyle::Display::Inline;
        } else if (tag == Hummingbird::Html::TagNames::Input && input_type_is_hidden(element)) {
            // `input[type=hidden]` carries form state (parent id, CSRF token, …)
            // and is never rendered — the UA sheet forces display:none. Without
            // this it fell through to the generic input branch and painted as a
            // stray 80x24 button box (seen on HN's comment form).
            style.display = ComputedStyle::Display::None;
        } else if (tag == Hummingbird::Html::TagNames::Input || tag == Hummingbird::Html::TagNames::Textarea ||
                   tag == Hummingbird::Html::TagNames::Button) {
            style.display = ComputedStyle::Display::InlineBlock;
        } else if (tag == Hummingbird::Html::TagNames::Li) {
            style.display = ComputedStyle::Display::ListItem;
        }
    }

    if (tag == Hummingbird::Html::TagNames::Ul || tag == Hummingbird::Html::TagNames::Ol) {
        style.padding.left = 20.0f;
        if (tag == Hummingbird::Html::TagNames::Ol) {
            style.list_style_type = ComputedStyle::ListStyleType::Decimal;
            overrides.list_style_type = true;
        } else {
            style.list_style_type = ComputedStyle::ListStyleType::Disc;
            overrides.list_style_type = true;
        }
    } else if (tag == Hummingbird::Html::TagNames::Pre) {
        if (style.whitespace == ComputedStyle::WhiteSpace::Normal) {
            style.whitespace = ComputedStyle::WhiteSpace::Preserve;
        }
        style.font_monospace = true;
        overrides.whitespace = true;
        overrides.font_monospace = true;
    } else if (tag == Hummingbird::Html::TagNames::A) {
        // A visited link prefers the document's vlink color (or the UA default
        // purple); otherwise the link color (or UA default blue). T-HIST-1.
        const bool visited = element.has_pseudo_state(DOM::Element::PseudoState::Visited);
        if (visited && parent_style && parent_style->vlink_color.has_value()) {
            style.color = *parent_style->vlink_color;
        } else if (parent_style && parent_style->link_color.has_value()) {
            style.color = *parent_style->link_color;
        } else if (visited) {
            style.color = {0x55, 0x1a, 0x8b, 255};
        } else {
            style.color = {0, 0, 255, 255};
        }
        style.underline = true;
        overrides.color = true;
        overrides.underline = true;
    } else if (tag == Hummingbird::Html::TagNames::Code) {
        style.font_monospace = true;
        style.padding.left = style.padding.right = 2.0f;
        style.padding.top = style.padding.bottom = 1.0f;
        overrides.font_monospace = true;
    } else if (tag == Hummingbird::Html::TagNames::Blockquote) {
        style.margin.left = 40.0f;
        style.margin.right = 40.0f;
        style.margin.top = 8.0f;
        style.margin.bottom = 8.0f;
    } else if (tag == Hummingbird::Html::TagNames::Hr) {
        style.height = ComputedStyle::LengthValue::from_px(2.0f);
        style.margin.top = style.margin.bottom = 8.0f;
        style.background = Color{50, 50, 50, 255};
    } else if (tag == Hummingbird::Html::TagNames::Input && input_type_is_toggle(element)) {
        // Checkbox/radio: a fixed 16x16 square with no padding or border — the
        // input painter draws the box, tick, and border itself. Explicit size
        // keeps flex/inline sizing from inflating it to a text-input rectangle.
        style.width = ComputedStyle::LengthValue::from_px(16.0f);
        style.height = ComputedStyle::LengthValue::from_px(16.0f);
        style.border_style = ComputedStyle::BorderStyle::None;
        style.border_width = {0.0f, 0.0f, 0.0f, 0.0f};
        style.margin.left = style.margin.right = 3.0f;
        style.margin.top = style.margin.bottom = 3.0f;
    } else if (tag == Hummingbird::Html::TagNames::Input) {
        style.border_style = ComputedStyle::BorderStyle::Solid;
        style.border_width = {1.0f, 1.0f, 1.0f, 1.0f};
        style.border_color = {125, 125, 125, 255};
        style.border_edge_color = {style.border_color, style.border_color, style.border_color, style.border_color};
        style.border_radius.set_all({2.0f, false});
        style.padding.left = 8.0f;
        style.padding.right = 8.0f;
        style.padding.top = 4.0f;
        style.padding.bottom = 4.0f;
        if (input_type_is_text_like(element)) {
            style.width = ComputedStyle::LengthValue::from_px(180.0f);
            style.height = ComputedStyle::LengthValue::from_px(24.0f);
            style.border_style = ComputedStyle::BorderStyle::Inset;
            style.background = Color{255, 255, 255, 255};
        } else {
            style.width = ComputedStyle::LengthValue::from_px(80.0f);
            style.height = ComputedStyle::LengthValue::from_px(24.0f);
            style.border_style = ComputedStyle::BorderStyle::Outset;
            style.background = Color{236, 236, 236, 255};
        }
    } else if (tag == Hummingbird::Html::TagNames::Textarea) {
        style.border_style = ComputedStyle::BorderStyle::Inset;
        style.border_width = {1.0f, 1.0f, 1.0f, 1.0f};
        style.border_color = {125, 125, 125, 255};
        style.border_edge_color = {style.border_color, style.border_color, style.border_color, style.border_color};
        style.border_radius.set_all({2.0f, false});
        style.padding.left = style.padding.right = 8.0f;
        style.padding.top = style.padding.bottom = kTextareaDefaultVerticalPadding;
        style.background = Color{255, 255, 255, 255};
        // Reserve one row per `rows`, and publish the row height as line_height so
        // DocumentInputPainter advances by exactly what was reserved instead of
        // guessing from font metrics. UA defaults run before inheritance, so the
        // basis is the parent's font size — the size this textarea will inherit
        // unless it is restyled. Author width/height/line-height still win by
        // normal cascade order.
        // KNOWN GAP (T-FORM-TEXTAREA-LAYOUT-1): an author `font-size` or
        // `line-height` set on the textarea itself changes the painted row height
        // but not this reserved box, so a `rows`-sized control can then hold
        // slightly fewer lines. `cols` is unimplemented for the same reason.
        if (style.line_height <= 0.0f) {
            const float basis = parent_style ? parent_style->font_size : style.font_size;
            style.line_height = basis * kTextareaLineHeightFactor;
        }
        style.width = ComputedStyle::LengthValue::from_px(kTextareaDefaultWidth);
        style.height = ComputedStyle::LengthValue::from_px(
            static_cast<float>(textarea_rows(element)) * style.line_height + 2.0f * kTextareaDefaultVerticalPadding);
    } else if (tag == Hummingbird::Html::TagNames::Button) {
        style.border_style = ComputedStyle::BorderStyle::Outset;
        style.border_width = {1.0f, 1.0f, 1.0f, 1.0f};
        style.border_color = {80, 80, 80, 255};
        style.border_edge_color = {style.border_color, style.border_color, style.border_color, style.border_color};
        style.border_radius.set_all({2.0f, false});
        style.padding.left = 10.0f;
        style.padding.right = 10.0f;
        style.padding.top = 4.0f;
        style.padding.bottom = 4.0f;
        style.background = Color{230, 230, 230, 255};
    } else if (tag == Hummingbird::Html::TagNames::Td || tag == Hummingbird::Html::TagNames::Th) {
        style.padding.left = style.padding.right = 2.0f;
        style.padding.top = style.padding.bottom = 2.0f;
    } else if (tag == Hummingbird::Html::TagNames::Strong || tag == Hummingbird::Html::TagNames::B) {
        style.weight = ComputedStyle::FontWeight::Bold;
        overrides.weight = true;
    } else if (tag == Hummingbird::Html::TagNames::Em || tag == Hummingbird::Html::TagNames::I) {
        style.style = ComputedStyle::FontStyle::Italic;
        overrides.style = true;
    } else if (tag == Hummingbird::Html::TagNames::H1) {
        set_heading(2.0f, 0.67f);
        overrides.font_size = true;
        overrides.weight = true;
    } else if (tag == Hummingbird::Html::TagNames::H2) {
        set_heading(1.5f, 0.83f);
        overrides.font_size = true;
        overrides.weight = true;
    } else if (tag == Hummingbird::Html::TagNames::H3) {
        set_heading(1.17f, 1.0f);
        overrides.font_size = true;
        overrides.weight = true;
    } else if (tag == Hummingbird::Html::TagNames::H4) {
        set_heading(1.0f, 1.33f);
        overrides.font_size = true;
        overrides.weight = true;
    } else if (tag == Hummingbird::Html::TagNames::H5) {
        set_heading(0.83f, 1.67f);
        overrides.font_size = true;
        overrides.weight = true;
    } else if (tag == Hummingbird::Html::TagNames::H6) {
        set_heading(0.67f, 2.33f);
        overrides.font_size = true;
        overrides.weight = true;
    }
}

void apply_legacy_attributes(const DOM::Element& element, ComputedStyle& style, StyleOverrides& overrides) {
    // `width`/`height` attributes here set only an absolute pixel computed width.
    // Percentages (`width="85%"`) are deliberately NOT turned into a computed
    // width: the table column planner reads those attributes itself (see the
    // PercentTable table-layout tests), and setting a computed width too would
    // double-handle and fight it.
    auto parse_length_value = [](std::string_view value) -> std::optional<float> {
        auto parsed = Core::Utils::parse_float(value, Core::Utils::NumberParseMode::Strict);
        if (!parsed) {
            return std::nullopt;
        }
        if (*parsed < 0.0f) {
            *parsed = 0.0f;
        }
        return parsed;
    };

    auto parse_font_size_value = [](std::string_view value) -> std::optional<float> {
        auto parsed = Core::Utils::parse_long(value, Core::Utils::NumberParseMode::Strict);
        if (!parsed) {
            return std::nullopt;
        }
        if (*parsed < 1) {
            *parsed = 1;
        }
        if (*parsed > 7) {
            *parsed = 7;
        }
        static constexpr float kFontSizes[] = {10.0f, 13.0f, 16.0f, 18.0f, 24.0f, 32.0f, 48.0f};
        return kFontSizes[*parsed - 1];
    };

    auto parse_font_face_value = [](std::string_view value) -> std::string {
        std::string_view trimmed = Core::Utils::trim_ascii_whitespace(value);
        if (auto comma = trimmed.find(','); comma != std::string_view::npos) {
            trimmed = Core::Utils::trim_ascii_whitespace(trimmed.substr(0, comma));
        }
        if (trimmed.size() >= 2 && ((trimmed.front() == '"' && trimmed.back() == '"') ||
                                    (trimmed.front() == '\'' && trimmed.back() == '\''))) {
            trimmed = trimmed.substr(1, trimmed.size() - 2);
        }
        return Core::Utils::to_lower(trimmed);
    };

    const auto& tag = element.get_tag_name();
    const bool is_body = tag == Hummingbird::Html::TagNames::Body;
    // `size` and `face` are presentational attributes of <font>/<basefont> only.
    // On other elements `size` means something else entirely — an <input>'s width
    // in characters, a <select>'s visible rows, an <hr>'s thickness — so mapping
    // it through the 1..7 font-size table there is wrong. HN's `<input size=20>`
    // was landing on the largest font (48px), overflowing the field.
    const bool is_font = tag == Hummingbird::Html::TagNames::Font;
    // `bgcolor` is a legacy presentational attribute of <body> AND the table
    // elements. HN's orange header bar is `<td bgcolor="#ff6600">`, so honoring it
    // only on <body> left the bar (and its background) unpainted.
    const bool is_table_bg_element =
        tag == Hummingbird::Html::TagNames::Table || tag == Hummingbird::Html::TagNames::Tr ||
        tag == Hummingbird::Html::TagNames::Td || tag == Hummingbird::Html::TagNames::Th ||
        tag == Hummingbird::Html::TagNames::Thead || tag == Hummingbird::Html::TagNames::Tbody ||
        tag == Hummingbird::Html::TagNames::Tfoot;
    for (const auto& [key, value] : element.get_attributes()) {
        if (key == Hummingbird::Html::AttributeNames::Align) {
            std::string normalized = Core::Utils::to_lower(value);
            if (normalized == "left") {
                style.text_align = ComputedStyle::TextAlign::Left;
                overrides.text_align = true;
            } else if (normalized == "center") {
                style.text_align = ComputedStyle::TextAlign::Center;
                overrides.text_align = true;
            } else if (normalized == "right") {
                style.text_align = ComputedStyle::TextAlign::Right;
                overrides.text_align = true;
            }
        } else if (key == Hummingbird::Html::AttributeNames::NoWrap) {
            style.whitespace = ComputedStyle::WhiteSpace::NoWrap;
            overrides.whitespace = true;
        } else if (key == Hummingbird::Html::AttributeNames::Width && !style.width.has_value()) {
            if (auto parsed = parse_length_value(value)) {
                style.width = ComputedStyle::LengthValue::from_px(*parsed);
            }
        } else if (key == Hummingbird::Html::AttributeNames::Height && !style.height.has_value()) {
            if (auto parsed = parse_length_value(value)) {
                style.height = ComputedStyle::LengthValue::from_px(*parsed);
            }
        } else if (key == Hummingbird::Html::AttributeNames::BgColor && (is_body || is_table_bg_element)) {
            if (auto parsed = Core::Utils::parse_html_color(value)) {
                style.background = *parsed;
                overrides.background = true;
            }
        } else if (key == Hummingbird::Html::AttributeNames::Size && is_font) {
            if (auto parsed = parse_font_size_value(value)) {
                style.font_size = *parsed;
                overrides.font_size = true;
            }
        } else if (key == Hummingbird::Html::AttributeNames::Face && is_font) {
            std::string face = parse_font_face_value(value);
            if (!face.empty()) {
                style.font_face = std::move(face);
                overrides.font_face = true;
            }
        } else if (is_body) {
            if (key == Hummingbird::Html::AttributeNames::Text) {
                if (auto parsed = Core::Utils::parse_html_color(value)) {
                    style.color = *parsed;
                    overrides.color = true;
                }
            } else if (key == Hummingbird::Html::AttributeNames::Link) {
                if (auto parsed = Core::Utils::parse_html_color(value)) {
                    style.link_color = *parsed;
                    overrides.link_color = true;
                }
            } else if (key == Hummingbird::Html::AttributeNames::VLink) {
                if (auto parsed = Core::Utils::parse_html_color(value)) {
                    style.vlink_color = *parsed;
                    overrides.vlink_color = true;
                }
            }
        }
    }
}

}  // namespace Hummingbird::Css::StyleDefaults
