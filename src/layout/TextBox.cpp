#include "layout/TextBox.h"

#include <algorithm>
#include <iostream>

#include "core/AssetPath.h"
#include "core/IGraphicsContext.h"

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
}  // namespace

void TextBox::layout(IGraphicsContext& context, const Rect& bounds) {
    m_rect.x = bounds.x;
    m_rect.y = bounds.y;

    const auto* style = get_computed_style();
    float padding_left = style ? style->padding.left : 0.0f;
    float padding_right = style ? style->padding.right : 0.0f;
    float padding_top = style ? style->padding.top : 0.0f;
    float padding_bottom = style ? style->padding.bottom : 0.0f;
    float border_left = style ? style->border_width.left : 0.0f;
    float border_right = style ? style->border_width.right : 0.0f;
    float border_top = style ? style->border_width.top : 0.0f;
    float border_bottom = style ? style->border_width.bottom : 0.0f;

    float inset_left = padding_left + border_left;
    float inset_right = padding_right + border_right;
    float inset_top = padding_top + border_top;
    float inset_bottom = padding_bottom + border_bottom;

    const std::string& text = get_dom_node()->get_text();
    if (style && style->whitespace == Css::ComputedStyle::WhiteSpace::Preserve) {
        m_rendered_text = text;
    } else {
        m_rendered_text = collapse_whitespace(text);
    }

    m_lines.clear();

    if (m_rendered_text.empty()) {
        m_lines.push_back("");
        m_last_metrics = {};
        m_rect.width = inset_left + inset_right;
        m_rect.height = inset_top + inset_bottom;
        return;
    }

    // Assumptions for now: font and size are hardcoded
    auto font_path = Hummingbird::resolve_asset_path("assets/fonts/Roboto-Regular.ttf");
    float font_size = style ? style->font_size : 16.0f;
    TextStyle text_style;
    text_style.font_path = font_path.string();
    text_style.font_size = font_size;
    text_style.bold = style && style->weight == Css::ComputedStyle::FontWeight::Bold;
    text_style.italic = style && style->style == Css::ComputedStyle::FontStyle::Italic;
    text_style.monospace = style && style->font_monospace;
    text_style.color = style ? style->color : Color{0, 0, 0, 255};

    // TODO: choose real monospace or bold fonts when available.
    m_last_metrics = context.measure_text(m_rendered_text, text_style);
    float line_height = m_last_metrics.height;

    float content_width = 0.0f;
    float available_width = bounds.width - inset_left - inset_right;
    if (available_width <= 0.0f) available_width = 0.0f;

    if (style && style->width.has_value()) {
        available_width = *style->width - inset_left - inset_right;
        if (available_width < 0.0f) available_width = 0.0f;
    }

    auto append_line = [&](std::string line_text, float measured_width) {
        m_lines.push_back(std::move(line_text));
        content_width = std::max(content_width, measured_width);
    };

    if (style && style->whitespace == Css::ComputedStyle::WhiteSpace::Preserve) {
        // Preserve newlines; no wrapping.
        size_t start = 0;
        while (start < m_rendered_text.size()) {
            size_t nl = m_rendered_text.find('\n', start);
            std::string line =
                nl == std::string::npos ? m_rendered_text.substr(start) : m_rendered_text.substr(start, nl - start);
            float w = context.measure_text(line, text_style).width;
            append_line(std::move(line), w);
            if (nl == std::string::npos) break;
            start = nl + 1;
        }
    } else {
        // Greedy wrap by tokens (words and explicit spaces) to preserve spacing around inline elements.
        std::vector<std::string> tokens;
        std::string current;
        for (char c : m_rendered_text) {
            if (c == ' ') {
                if (!current.empty()) {
                    tokens.push_back(current);
                    current.clear();
                }
                tokens.emplace_back(" ");
            } else {
                current.push_back(c);
            }
        }
        if (!current.empty()) {
            tokens.push_back(current);
        }
        if (tokens.empty()) {
            tokens.emplace_back(" ");
        }

        auto measure_word = [&](const std::string& w) { return context.measure_text(w, text_style).width; };
        float space_width = context.measure_text(" ", text_style).width;

        std::string line_text;
        float line_width = 0.0f;
        for (const auto& tok : tokens) {
            bool is_space = tok == " ";
            float tok_width = is_space ? space_width : measure_word(tok);
            bool would_overflow =
                (available_width > 0.0f && line_width > 0.0f && (line_width + tok_width) > available_width);
            if (would_overflow) {
                append_line(line_text, line_width);
                line_text.clear();
                line_width = 0.0f;
                if (is_space) {
                    continue;  // drop leading space on new line
                }
            }
            line_text += tok;
            line_width += tok_width;
        }
        append_line(line_text, line_width);
    }

    m_rect.height = static_cast<float>(m_lines.size()) * line_height + inset_top + inset_bottom;

    if (content_width == 0.0f) {
        content_width = m_last_metrics.width;
    }

    m_rect.width = content_width + inset_left + inset_right;
    if (m_rect.height == 0.0f) {
        m_rect.height = line_height + inset_top + inset_bottom;
    }

    if (m_last_metrics.width == 0 || m_last_metrics.height == 0) {
        std::cerr << "[TextBox::layout] zero metrics for text '" << m_rendered_text << "' using font " << font_path
                  << "\n";
    }
}

void TextBox::paint(IGraphicsContext& context, const Point& offset) {
    // The absolute position to draw the text is the parent's offset plus our own relative position.
    const auto* style = get_computed_style();
    float padding_left = style ? style->padding.left : 0.0f;
    float padding_top = style ? style->padding.top : 0.0f;
    float border_left = style ? style->border_width.left : 0.0f;
    float border_top = style ? style->border_width.top : 0.0f;

    float inset_left = padding_left + border_left;
    float inset_top = padding_top + border_top;

    float absolute_x = offset.x + m_rect.x + inset_left;
    float absolute_y = offset.y + m_rect.y + inset_top;

    if (m_lines.empty()) return;

    TextStyle text_style;
    auto font_path = Hummingbird::resolve_asset_path("assets/fonts/Roboto-Regular.ttf");
    text_style.font_path = font_path.string();
    text_style.font_size = style ? style->font_size : 16.0f;
    text_style.bold = style && style->weight == Css::ComputedStyle::FontWeight::Bold;
    text_style.italic = style && style->style == Css::ComputedStyle::FontStyle::Italic;
    text_style.monospace = style && style->font_monospace;
    text_style.color = style ? style->color : Color{0, 0, 0, 255};

    if (style && style->background.has_value()) {
        Hummingbird::Layout::Rect bg{offset.x + m_rect.x, offset.y + m_rect.y, m_rect.width, m_rect.height};
        context.fill_rect(bg, *style->background);
    }

    float line_height = m_last_metrics.height;
    float underline_width = 0.0f;
    for (size_t i = 0; i < m_lines.size(); ++i) {
        float y = absolute_y + static_cast<float>(i) * line_height;
        if (!m_lines[i].empty()) {
            context.draw_text(m_lines[i], absolute_x, y, text_style);
            underline_width = std::max(underline_width, context.measure_text(m_lines[i], text_style).width);
        } else {
            // Empty line: still advance height, but no draw/measure.
            underline_width = std::max(underline_width, 0.0f);
        }
    }

    if (style && style->underline && underline_width > 0) {
        float underline_y = absolute_y + static_cast<float>(m_lines.size()) * line_height - 2.0f;
        Hummingbird::Layout::Rect line_rect{absolute_x, underline_y, underline_width, 1.0f};
        context.fill_rect(line_rect, text_style.color);
    }

    // TextBoxes don't have children, so no need to call base class paint.
}

}  // namespace Hummingbird::Layout
