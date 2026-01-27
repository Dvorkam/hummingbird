#include "layout/TextBox.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <optional>
#include <ostream>
#include <utility>

#include "core/platform_api/IGraphicsContext.h"
#include "layout/LayoutMetricsUtils.h"
#include "layout/TextStyleUtils.h"
#include "style/ComputedStyle.h"

namespace Hummingbird::Layout {

TextBox::TextBox(const DOM::Text* dom_node) : RenderObject(dom_node) {}

namespace {
constexpr float kInlineMeasurementWidth = 100000.0f;
constexpr float kDefaultFontSizePx = 16.0f;
constexpr float kUnderlineOffsetPx = 2.0f;
constexpr float kUnderlineThicknessPx = 1.0f;

using Metrics::Insets;

struct UnderlineMetrics {
    float position = 0.0f;
    float thickness = kUnderlineThicknessPx;
};

UnderlineMetrics resolve_underline_metrics(const TextMetrics& metrics) {
    UnderlineMetrics underline;
    float position = -metrics.underline_position;
    float fallback = metrics.descent > 0.0f ? std::max(1.0f, metrics.descent * 0.25f) : kUnderlineOffsetPx;
    if (position <= 0.0f) {
        position = fallback;
    }
    if (metrics.descent > 0.0f) {
        position = std::min(position, metrics.descent);
    }
    underline.position = position;
    underline.thickness = metrics.underline_thickness > 0.0f ? metrics.underline_thickness : kUnderlineThicknessPx;
    if (underline.thickness < kUnderlineThicknessPx) {
        underline.thickness = kUnderlineThicknessPx;
    }
    return underline;
}

float compute_underline_y(float line_top, float line_height, const TextMetrics& metrics,
                          const UnderlineMetrics& underline) {
    if (metrics.ascent > 0.0f) {
        return line_top + metrics.ascent + underline.position;
    }
    return line_top + line_height - kUnderlineOffsetPx;
}

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

std::vector<std::string> tokenize_text(const std::string& text) {
    std::vector<std::string> tokens;
    std::string current;
    for (char c : text) {
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
    return tokens;
}

std::string build_rendered_text(const std::string& text, const Css::ComputedStyle* style) {
    if (style && style->whitespace == Css::ComputedStyle::WhiteSpace::Preserve) {
        return text;
    }
    return collapse_whitespace(text);
}

float compute_available_width(const Css::ComputedStyle* style, const Rect& bounds, const Insets& insets) {
    float available_width = bounds.width - insets.left - insets.right;
    if (available_width <= 0.0f) {
        available_width = 0.0f;
    }

    if (style && style->width.has_value()) {
        available_width = *style->width - insets.left - insets.right;
        if (available_width < 0.0f) {
            available_width = 0.0f;
        }
    }

    return available_width;
}

void append_line(std::vector<std::string>& lines, std::vector<float>& line_widths, float& content_width,
                 std::string line_text, float measured_width) {
    lines.push_back(std::move(line_text));
    line_widths.push_back(measured_width);
    content_width = std::max(content_width, measured_width);
}

void build_preserved_lines(IGraphicsContext& context, const std::string& text, const TextStyle& text_style,
                           std::vector<std::string>& lines, std::vector<float>& line_widths, float& content_width) {
    // Preserve newlines; no wrapping.
    size_t start = 0;
    while (start < text.size()) {
        size_t nl = text.find('\n', start);
        std::string line = nl == std::string::npos ? text.substr(start) : text.substr(start, nl - start);
        float w = context.measure_text(line, text_style).width;
        append_line(lines, line_widths, content_width, std::move(line), w);
        if (nl == std::string::npos) {
            break;
        }
        start = nl + 1;
    }
}

void build_wrapped_lines(IGraphicsContext& context, const std::string& text, const TextStyle& text_style,
                         float available_width, std::vector<std::string>& lines, std::vector<float>& line_widths,
                         float& content_width) {
    // Greedy wrap by tokens (words and explicit spaces) to preserve spacing around inline elements.
    auto tokens = tokenize_text(text);

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
            append_line(lines, line_widths, content_width, line_text, line_width);
            line_text.clear();
            line_width = 0.0f;
            if (is_space) {
                continue;  // drop leading space on new line
            }
        }
        line_text += tok;
        line_width += tok_width;
    }
    append_line(lines, line_widths, content_width, line_text, line_width);
}

bool apply_empty_text_layout(const std::string& rendered_text, std::vector<std::string>& lines,
                             std::vector<float>& line_widths, TextMetrics& last_metrics, float& line_height, Rect& rect,
                             const Insets& insets) {
    if (!rendered_text.empty()) {
        return false;
    }

    lines.push_back("");
    line_widths.push_back(0.0f);
    last_metrics = {};
    line_height = 0.0f;
    rect.width = insets.left + insets.right;
    rect.height = insets.top + insets.bottom;
    return true;
}

float measure_text_block(IGraphicsContext& context, const std::string& rendered_text, const TextStyle& text_style,
                         TextMetrics& last_metrics) {
    last_metrics = context.measure_text(rendered_text, text_style);
    return last_metrics.height;
}
}  // namespace

void TextBox::layout(IGraphicsContext& context, const Rect& bounds) {
    m_rect.x = bounds.x;
    m_rect.y = bounds.y;

    const auto* style = get_computed_style();
    Insets insets = Metrics::compute_insets(style);

    m_rendered_text = build_rendered_text(get_dom_node()->get_text(), style);

    m_lines.clear();
    m_line_widths.clear();
    m_line_height = 0.0f;

    if (apply_empty_text_layout(m_rendered_text, m_lines, m_line_widths, m_last_metrics, m_line_height, m_rect,
                                insets)) {
        return;
    }

    // Assumptions for now: monospace font selection is still hardcoded.
    float font_size = style ? style->font_size : kDefaultFontSizePx;
    TextStyle text_style = TextStyleUtils::build_text_style(style);
    text_style.font_size = font_size;

    if (text_style.monospace) {
        // TODO: choose real monospace fonts when available.
        static std::atomic<bool> warned{false};
        if (!warned.exchange(true, std::memory_order_relaxed)) {
            HB_LOG_WARN("[layout] Not implemented: real monospace font selection");
        }
    }
    float line_height = measure_text_block(context, m_rendered_text, text_style, m_last_metrics);
    if (style && style->line_height > 0.0f) {
        line_height = style->line_height;
    }
    m_line_height = line_height;

    float content_width = 0.0f;
    float available_width = compute_available_width(style, bounds, insets);

    if (style && style->whitespace == Css::ComputedStyle::WhiteSpace::Preserve) {
        build_preserved_lines(context, m_rendered_text, text_style, m_lines, m_line_widths, content_width);
    } else if (style && style->whitespace == Css::ComputedStyle::WhiteSpace::NoWrap) {
        build_wrapped_lines(context, m_rendered_text, text_style, 0.0f, m_lines, m_line_widths, content_width);
    } else {
        build_wrapped_lines(context, m_rendered_text, text_style, available_width, m_lines, m_line_widths,
                            content_width);
    }

    m_rect.height = static_cast<float>(m_lines.size()) * line_height + insets.top + insets.bottom;

    if (content_width == 0.0f) {
        content_width = m_last_metrics.width;
    }

    m_rect.width = content_width + insets.left + insets.right;
    if (m_rect.height == 0.0f) {
        m_rect.height = line_height + insets.top + insets.bottom;
    }

    if (m_last_metrics.width == 0 || m_last_metrics.height == 0) {
        HB_LOG_WARN("[layout] zero metrics for text '" << m_rendered_text << "' using font " << text_style.font_path);
    }
}

void TextBox::reset_inline_layout() {
    m_fragments.clear();
    m_lines.clear();
    m_line_widths.clear();
    m_line_height = 0.0f;
    m_inline_runs.clear();
}

void TextBox::measure_inline(IGraphicsContext& context) {
    m_inline_runs.clear();
    const auto* style = get_computed_style();
    const std::string& text = get_dom_node()->get_text();
    if (style && style->whitespace == Css::ComputedStyle::WhiteSpace::Preserve) {
        layout(context, {0.0f, 0.0f, kInlineMeasurementWidth, 0.0f});
        InlineRun run;
        run.owner = this;
        run.local_index = 0;
        run.text = m_rendered_text;
        run.width = m_rect.width;
        run.height = m_rect.height;
        m_inline_runs.push_back(std::move(run));
        return;
    }

    m_rendered_text = collapse_whitespace(text);
    auto tokens = tokenize_text(m_rendered_text);
    TextStyle text_style = TextStyleUtils::build_text_style(style);
    m_last_metrics = context.measure_text("A", text_style);
    float line_height = m_last_metrics.height;
    if (style && style->line_height > 0.0f) {
        line_height = style->line_height;
    }
    m_line_height = line_height;
    m_fragments.clear();
    m_fragments.resize(tokens.size());

    m_inline_runs.reserve(tokens.size());
    for (size_t i = 0; i < tokens.size(); ++i) {
        InlineRun run;
        run.owner = this;
        run.local_index = i;
        run.text = tokens[i];
        run.width = context.measure_text(tokens[i], text_style).width;
        run.height = line_height;
        m_inline_runs.push_back(std::move(run));
    }
}

void TextBox::collect_inline_runs(IGraphicsContext& /*context*/, std::vector<InlineRun>& runs) {
    runs.insert(runs.end(), m_inline_runs.begin(), m_inline_runs.end());
}

void TextBox::apply_inline_fragment(size_t index, const InlineFragment& fragment, const InlineRun& run) {
    if (index >= m_fragments.size()) {
        m_fragments.resize(index + 1);
    }
    m_fragments[index].text = run.text;
    m_fragments[index].rect = fragment.rect;
    m_fragments[index].line_index = fragment.line_index;
}

void TextBox::finalize_inline_layout() {
    if (m_fragments.empty()) {
        m_rect = {};
        return;
    }

    float min_x = m_fragments[0].rect.x;
    float min_y = m_fragments[0].rect.y;
    float max_x = m_fragments[0].rect.x + m_fragments[0].rect.width;
    float max_y = m_fragments[0].rect.y + m_fragments[0].rect.height;

    for (const auto& frag : m_fragments) {
        min_x = std::min(min_x, frag.rect.x);
        min_y = std::min(min_y, frag.rect.y);
        max_x = std::max(max_x, frag.rect.x + frag.rect.width);
        max_y = std::max(max_y, frag.rect.y + frag.rect.height);
    }

    m_rect.x = min_x;
    m_rect.y = min_y;
    m_rect.width = max_x - min_x;
    m_rect.height = max_y - min_y;

    for (auto& frag : m_fragments) {
        frag.rect.x -= min_x;
        frag.rect.y -= min_y;
    }
}

void TextBox::paint_self(IGraphicsContext& context, const Point& offset) const {
    // The absolute position to draw the text is the parent's offset plus our own relative position.
    const auto* style = get_computed_style();
    Insets insets = Metrics::compute_insets(style);

    float absolute_x = offset.x + m_rect.x + insets.left;
    float absolute_y = offset.y + m_rect.y + insets.top;

    TextStyle text_style = TextStyleUtils::build_text_style(style);

    if (style && style->background.has_value()) {
        Hummingbird::Layout::Rect bg{offset.x + m_rect.x, offset.y + m_rect.y, m_rect.width, m_rect.height};
        context.fill_rect(bg, *style->background);
    }

    if (!(style && style->background.has_value())) {
        if (const auto* parent = get_parent()) {
            const auto* parent_style = parent->get_computed_style();
            if (parent_style && parent_style->background.has_value() && parent_style->font_monospace &&
                parent_style->display == Css::ComputedStyle::Display::Inline) {
                const auto& parent_rect = parent->get_rect();
                Hummingbird::Layout::Rect bg{offset.x, offset.y, parent_rect.width, parent_rect.height};
                context.fill_rect(bg, *parent_style->background);
            }
        }
    }

    if (!m_fragments.empty()) {
        float line_height = m_line_height > 0.0f ? m_line_height : m_last_metrics.height;
        paint_fragments(context, text_style, absolute_x, absolute_y, line_height, style && style->underline);
        return;
    }

    if (m_lines.empty()) return;

    paint_lines(context, text_style, absolute_x, absolute_y, style && style->underline);
}

void TextBox::paint_fragments(IGraphicsContext& context, const TextStyle& text_style, float absolute_x,
                              float absolute_y, float line_height, bool underline) const {
    TextMetrics line_metrics = m_last_metrics;
    if (line_metrics.height <= 0.0f) {
        line_metrics = context.measure_text("A", text_style);
    }
    UnderlineMetrics underline_metrics = resolve_underline_metrics(line_metrics);

    std::vector<float> line_widths;
    for (const auto& frag : m_fragments) {
        size_t line_index = frag.line_index;
        if (line_index >= line_widths.size()) {
            line_widths.resize(line_index + 1, 0.0f);
        }
        float line_right = frag.rect.x + frag.rect.width;
        line_widths[line_index] = std::max(line_widths[line_index], line_right);
        TextMetrics metrics{frag.rect.width, frag.rect.height};
        context.draw_text_with_metrics(frag.text, absolute_x + frag.rect.x, absolute_y + frag.rect.y, text_style,
                                       metrics);
    }

    if (!underline) {
        return;
    }

    for (size_t i = 0; i < line_widths.size(); ++i) {
        if (line_widths[i] <= 0.0f) {
            continue;
        }
        float line_top = absolute_y + static_cast<float>(i) * line_height;
        float underline_y = compute_underline_y(line_top, line_height, line_metrics, underline_metrics);
        Hummingbird::Layout::Rect line_rect{absolute_x, underline_y, line_widths[i], underline_metrics.thickness};
        context.fill_rect(line_rect, text_style.color);
    }
}

void TextBox::paint_lines(IGraphicsContext& context, const TextStyle& text_style, float absolute_x, float absolute_y,
                          bool underline) const {
    float line_height = m_line_height > 0.0f ? m_line_height : m_last_metrics.height;
    TextMetrics line_metrics = m_last_metrics;
    if (line_metrics.height <= 0.0f) {
        line_metrics = context.measure_text("A", text_style);
    }
    UnderlineMetrics underline_metrics = resolve_underline_metrics(line_metrics);
    for (size_t i = 0; i < m_lines.size(); ++i) {
        float line_top = absolute_y + static_cast<float>(i) * line_height;
        float line_width = 0.0f;
        if (i < m_line_widths.size()) {
            line_width = m_line_widths[i];
        }
        if (!m_lines[i].empty()) {
            TextMetrics metrics{line_width, line_height};
            context.draw_text_with_metrics(m_lines[i], absolute_x, line_top, text_style, metrics);
        }
        if (underline && line_width > 0.0f) {
            float underline_y = compute_underline_y(line_top, line_height, line_metrics, underline_metrics);
            Hummingbird::Layout::Rect line_rect{absolute_x, underline_y, line_width, underline_metrics.thickness};
            context.fill_rect(line_rect, text_style.color);
        }
    }
}

}  // namespace Hummingbird::Layout
