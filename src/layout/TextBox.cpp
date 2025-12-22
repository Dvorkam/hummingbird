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
    // Trim leading/trailing spaces
    while (!out.empty() && out.front() == ' ') {
        out.erase(out.begin());
    }
    while (!out.empty() && out.back() == ' ') {
        out.pop_back();
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
    float line_height = m_last_metrics.height;

    float content_width = 0.0f;
    float available_width = bounds.width - padding_left - padding_right;
    if (available_width <= 0.0f) available_width = 0.0f;

    if (style && style->width.has_value()) {
        available_width = *style->width - padding_left - padding_right;
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
            std::string line = nl == std::string::npos ? m_rendered_text.substr(start)
                                                       : m_rendered_text.substr(start, nl - start);
            float w = context.measure_text(line, font_path.string(), font_size, bold, italic, monospace).width;
            append_line(std::move(line), w);
            if (nl == std::string::npos) break;
            start = nl + 1;
        }
    } else {
        // Greedy wrap by words.
        std::vector<std::string> words;
        std::string current;
        for (char c : m_rendered_text) {
            if (c == ' ') {
                if (!current.empty()) {
                    words.push_back(current);
                    current.clear();
                }
            } else {
                current.push_back(c);
            }
        }
        if (!current.empty()) {
            words.push_back(current);
        }

        auto measure_word = [&](const std::string& w) {
            return context.measure_text(w, font_path.string(), font_size, bold, italic, monospace).width;
        };
        float space_width = context.measure_text(" ", font_path.string(), font_size, bold, italic, monospace).width;

        std::string line_text;
        float line_width = 0.0f;
        for (size_t i = 0; i < words.size(); ++i) {
            const auto& w = words[i];
            float w_width = measure_word(w);
            float sep = line_width > 0.0f ? space_width : 0.0f;
            float projected = line_width + sep + w_width;
            if (line_width > 0.0f && projected > available_width && available_width > 0.0f) {
                append_line(line_text, line_width);
                line_text = w;
                line_width = w_width;
            } else {
                if (sep > 0.0f) {
                    line_text.push_back(' ');
                    line_width += sep;
                }
                line_text += w;
                line_width += w_width;
            }
        }
        append_line(line_text, line_width);
    }

    m_rect.height = static_cast<float>(m_lines.size()) * line_height + padding_top + padding_bottom;

    if (content_width == 0.0f) {
        content_width = m_last_metrics.width;
    }

    m_rect.width = content_width + padding_left + padding_right;
    if (m_rect.height == 0.0f) {
        m_rect.height = line_height + padding_top + padding_bottom;
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

    float absolute_x = offset.x + m_rect.x + padding_left;
    float absolute_y = offset.y + m_rect.y + padding_top;

    if (m_lines.empty()) return;

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

    float line_height = m_last_metrics.height;
    float underline_width = 0.0f;
    for (size_t i = 0; i < m_lines.size(); ++i) {
        float y = absolute_y + static_cast<float>(i) * line_height;
        context.draw_text(m_lines[i], absolute_x, y, font_path.string(), font_size, text_color, bold, italic,
                          monospace);
        underline_width = std::max(underline_width, context.measure_text(m_lines[i], font_path.string(), font_size, bold,
                                                                         italic, monospace)
                                                       .width);
    }

    if (style && style->underline && underline_width > 0) {
        float underline_y = absolute_y + static_cast<float>(m_lines.size()) * line_height - 2.0f;
        Hummingbird::Layout::Rect line_rect{absolute_x, underline_y, underline_width, 1.0f};
        context.fill_rect(line_rect, text_color);
    }

    // TextBoxes don't have children, so no need to call base class paint.
}

}  // namespace Hummingbird::Layout
