#include "layout/TextBox.h"
#include "core/IGraphicsContext.h"
#include "core/AssetPath.h"
#include <iostream>
#include <algorithm>

namespace Hummingbird::Layout {

TextBox::TextBox(const DOM::Text* dom_node) : RenderObject(dom_node) {}

namespace {
// Collapse runs of whitespace to a single space; convert newlines/tabs to spaces.
std::string collapse_whitespace(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    bool in_space = false;
    for (char c : text) {
        bool is_space = c == ' ' || c == '\n' || c == '\r' || c == '\t';
        if (is_space) {
            if (!in_space) {
                out.push_back(' ');
            }
            in_space = true;
        } else {
            out.push_back(c);
            in_space = false;
        }
    }
    return out;
}
}

void TextBox::layout(IGraphicsContext& context, const Rect& bounds) {
    m_rect.x = bounds.x;
    m_rect.y = bounds.y;

    const auto* style = get_computed_style();
    float padding_left = style ? style->padding.left : 0.0f;
    float padding_right = style ? style->padding.right : 0.0f;
    float padding_top = style ? style->padding.top : 0.0f;
    float padding_bottom = style ? style->padding.bottom : 0.0f;

    const std::string& text = get_dom_node()->get_text();
    if (style && style->whitespace == Css::ComputedStyle::WhiteSpace::Preserve) {
        m_rendered_text = text;
    } else {
        m_rendered_text = collapse_whitespace(text);
    }

    if (m_rendered_text.empty()) {
        m_last_metrics = {};
        m_rect.width = padding_left + padding_right;
        m_rect.height = padding_top + padding_bottom;
        return;
    }

    // Assumptions for now: font and size are hardcoded
    auto font_path = Hummingbird::resolve_asset_path("assets/fonts/Roboto-Regular.ttf");
    float font_size = style ? style->font_size : 16.0f;
    bool bold = style && style->weight == Css::ComputedStyle::FontWeight::Bold;
    bool italic = style && style->style == Css::ComputedStyle::FontStyle::Italic;
    bool monospace = style && style->font_monospace;

    // TODO: choose real monospace or bold fonts when available.
    m_last_metrics = context.measure_text(m_rendered_text, font_path.string(), font_size, bold, italic, monospace);
    float content_width = m_last_metrics.width;
    if (style && style->width.has_value()) {
        content_width = *style->width;
    }

    m_rect.width = content_width + padding_left + padding_right;
    m_rect.height = m_last_metrics.height + padding_top + padding_bottom;

    if (m_last_metrics.width == 0 || m_last_metrics.height == 0) {
        std::cerr << "[TextBox::layout] zero metrics for text '" << m_rendered_text << "' using font " << font_path << "\n";
    }
}

void TextBox::paint(IGraphicsContext& context, const Point& offset) {
    // The absolute position to draw the text is the parent's offset plus our own relative position.
    const auto* style = get_computed_style();
    float padding_left = style ? style->padding.left : 0.0f;
    float padding_top = style ? style->padding.top : 0.0f;

    float absolute_x = offset.x + m_rect.x + padding_left;
    float absolute_y = offset.y + m_rect.y + padding_top;

    if (m_rendered_text.empty()) return;

    Color text_color = style ? style->color : Color{0, 0, 0, 255};
    bool bold = style && style->weight == Css::ComputedStyle::FontWeight::Bold;
    bool italic = style && style->style == Css::ComputedStyle::FontStyle::Italic;
    bool monospace = style && style->font_monospace;

    if (style && style->background.has_value()) {
        Hummingbird::Layout::Rect bg{offset.x + m_rect.x, offset.y + m_rect.y, m_rect.width, m_rect.height};
        context.fill_rect(bg, *style->background);
    }

    // Assumptions for now: font selection is basic; just adjust size.
    auto font_path = Hummingbird::resolve_asset_path("assets/fonts/Roboto-Regular.ttf");
    float font_size = style ? style->font_size : 16.0f;
    
    context.draw_text(m_rendered_text, absolute_x, absolute_y, font_path.string(), font_size, text_color, bold, italic, monospace);

    if (style && style->underline && m_last_metrics.width > 0) {
        float underline_y = absolute_y + m_last_metrics.height - 2.0f;
        Hummingbird::Layout::Rect line_rect{absolute_x, underline_y, m_last_metrics.width, 1.0f};
        context.fill_rect(line_rect, text_color);
    }

    // TextBoxes don't have children, so no need to call base class paint.
}

}
