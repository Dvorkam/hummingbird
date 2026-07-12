#include "layout/flow/TextBox.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <optional>
#include <ostream>
#include <utility>

#include "core/platform_api/IGraphicsContext.h"
#include "core/utils/Utf8Utils.h"
#include "layout/flow/TextDecorationUtils.h"
#include "layout/flow/TextLayoutUtils.h"
#include "layout/flow/TextMeasurer.h"
#include "layout/flow/TextOverflowUtils.h"
#include "layout/flow/TextStyleUtils.h"
#include "layout/flow/inline/InlineVerticalAlignUtils.h"
#include "layout/geometry/metrics/LayoutMetricsUtils.h"
#include "layout/geometry/metrics/TextMetricsUtils.h"
#include "style/types/ComputedStyle.h"

namespace Hummingbird::Layout {

TextBox::TextBox(const DOM::Text* dom_node) : RenderObject(dom_node) {}

namespace {
constexpr float kInlineMeasurementWidth = 100000.0f;
constexpr float kDefaultFontSizePx = 16.0f;

using Metrics::Insets;

void append_line(std::vector<std::string>& lines, std::vector<float>& line_widths, float& content_width,
                 std::string line_text, float measured_width) {
    lines.push_back(std::move(line_text));
    line_widths.push_back(measured_width);
    content_width = std::max(content_width, measured_width);
}

void draw_spaced_text(IGraphicsContext& context, std::string_view text, float x, float y, const TextStyle& text_style,
                      float letter_spacing) {
    if (text.empty()) {
        return;
    }
    if (letter_spacing == 0.0f) {
        context.draw_text(std::string(text), x, y, text_style);
        return;
    }
    float cursor_x = x;
    size_t index = 0;
    while (index < text.size()) {
        size_t next = Core::Utils::next_codepoint(text, index);
        if (next <= index) {
            break;
        }
        std::string glyph(text.substr(index, next - index));
        TextMetrics glyph_metrics = context.measure_text(glyph, text_style);
        context.draw_text(glyph, cursor_x, y, text_style);
        cursor_x += glyph_metrics.width;
        if (next < text.size()) {
            cursor_x += letter_spacing;
        }
        index = next;
    }
}

std::string split_token_by_width(IGraphicsContext& context, std::string_view token, const TextStyle& text_style,
                                 float letter_spacing, float max_width, std::string& remainder) {
    remainder.clear();
    if (token.empty() || max_width <= 0.0f) {
        remainder = std::string(token);
        return "";
    }
    float width = 0.0f;
    TextMeasurer glyph_measurer(context, text_style, 0.0f);
    std::string head;
    size_t index = 0;
    while (index < token.size()) {
        size_t next = Core::Utils::next_codepoint(token, index);
        if (next <= index) {
            break;
        }
        std::string_view glyph_view = token.substr(index, next - index);
        float glyph_width = glyph_measurer.measure(glyph_view);
        float spacing = head.empty() ? 0.0f : letter_spacing;
        if (width + spacing + glyph_width > max_width) {
            remainder = std::string(token.substr(index));
            return head;
        }
        if (!head.empty()) {
            width += letter_spacing;
        }
        head.append(glyph_view.begin(), glyph_view.end());
        width += glyph_width;
        index = next;
    }
    return head;
}

void build_preserved_lines(IGraphicsContext& context, const std::string& text, const TextStyle& text_style,
                           std::vector<std::string>& lines, std::vector<float>& line_widths, float& content_width,
                           float letter_spacing) {
    TextMeasurer measurer(context, text_style, letter_spacing);
    // Preserve newlines; no wrapping.
    size_t start = 0;
    while (start < text.size()) {
        size_t nl = text.find('\n', start);
        std::string line = nl == std::string::npos ? text.substr(start) : text.substr(start, nl - start);
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();  // CRLF sources must not leak \r glyphs into preserved lines.
        }
        float w = measurer.measure(line);
        append_line(lines, line_widths, content_width, std::move(line), w);
        if (nl == std::string::npos) {
            break;
        }
        start = nl + 1;
    }
}

void build_wrapped_lines(IGraphicsContext& context, const std::string& text, const TextStyle& text_style,
                         float available_width, std::vector<std::string>& lines, std::vector<float>& line_widths,
                         float& content_width, float letter_spacing, bool break_long_words) {
    // Greedy wrap by tokens (words and explicit spaces) to preserve spacing around inline elements.
    auto tokens = TextLayoutUtils::tokenize_text(text);

    TextMeasurer measurer(context, text_style, letter_spacing);
    TextMeasurer glyph_measurer(context, text_style, 0.0f);
    auto measure_word = [&](const std::string& w) { return measurer.measure(w); };
    float space_width = glyph_measurer.measure(" ");

    std::string line_text;
    float line_width = 0.0f;
    for (const auto& tok : tokens) {
        bool is_space = tok == " ";
        float tok_width = is_space ? space_width : measure_word(tok);
        bool would_overflow = (available_width > 0.0f && (line_width + tok_width) > available_width);
        if (would_overflow) {
            if (!is_space && break_long_words && available_width > 0.0f && line_width == 0.0f) {
                std::string remaining = tok;
                while (!remaining.empty()) {
                    std::string tail;
                    std::string head =
                        split_token_by_width(context, remaining, text_style, letter_spacing, available_width, tail);
                    if (head.empty()) {
                        head = tail.substr(0, 1);
                        tail = tail.substr(head.size());
                    }
                    float head_width = measure_word(head);
                    append_line(lines, line_widths, content_width, head, head_width);
                    remaining = tail;
                }
                continue;
            }
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

float compute_text_ascent(const TextMetrics& metrics, float line_height) {
    return TextMetricsUtils::resolve_text_ascent(metrics, line_height);
}
}  // namespace

void TextBox::layout(IGraphicsContext& context, const Rect& bounds) {
    m_rect.x = bounds.x;
    m_rect.y = bounds.y;

    const auto* style = get_computed_style();
    Insets insets = Metrics::compute_insets(style);

    m_rendered_text = TextLayoutUtils::build_rendered_text(get_dom_node()->get_text(), style);

    m_lines.clear();
    m_line_widths.clear();
    m_line_x_offsets.clear();
    m_line_height = 0.0f;

    if (apply_empty_text_layout(m_rendered_text, m_lines, m_line_widths, m_last_metrics, m_line_height, m_rect,
                                insets)) {
        return;
    }

    float font_size = style ? style->font_size : kDefaultFontSizePx;
    TextStyle text_style = TextStyleUtils::build_text_style(style);
    text_style.font_size = font_size;
    float letter_spacing = style ? style->letter_spacing : 0.0f;

    float line_height = measure_text_block(context, m_rendered_text, text_style, m_last_metrics);
    line_height = TextMetricsUtils::resolve_line_height(style, line_height);
    m_line_height = line_height;

    float content_width = 0.0f;
    float available_width = Metrics::compute_available_width(style, bounds, insets);
    if (style && style->whitespace == Css::ComputedStyle::WhiteSpace::Preserve) {
        build_preserved_lines(context, m_rendered_text, text_style, m_lines, m_line_widths, content_width,
                              letter_spacing);
    } else if (style && style->whitespace == Css::ComputedStyle::WhiteSpace::NoWrap) {
        build_wrapped_lines(context, m_rendered_text, text_style, 0.0f, m_lines, m_line_widths, content_width,
                            letter_spacing, false);
    } else {
        build_wrapped_lines(context, m_rendered_text, text_style, available_width, m_lines, m_line_widths,
                            content_width, letter_spacing,
                            style && style->word_wrap == Css::ComputedStyle::WordWrap::BreakWord);
        if (m_lines.size() > 1 && available_width > 0.0f) {
            std::vector<std::string> tail_lines;
            std::vector<float> tail_widths;
            float tail_content_width = 0.0f;
            std::string tail_text;
            for (size_t i = 1; i < m_lines.size(); ++i) {
                if (!tail_text.empty()) {
                    tail_text.push_back(' ');
                }
                tail_text += m_lines[i];
            }
            if (!tail_text.empty()) {
                build_wrapped_lines(context, tail_text, text_style, available_width, tail_lines, tail_widths,
                                    tail_content_width, letter_spacing,
                                    style && style->word_wrap == Css::ComputedStyle::WordWrap::BreakWord);
                m_lines.resize(1);
                m_line_widths.resize(1);
                m_lines.insert(m_lines.end(), tail_lines.begin(), tail_lines.end());
                m_line_widths.insert(m_line_widths.end(), tail_widths.begin(), tail_widths.end());
                content_width = std::max(content_width, tail_content_width);
            }
        }
    }

    if (style && style->whitespace == Css::ComputedStyle::WhiteSpace::NoWrap &&
        style->text_overflow == Css::ComputedStyle::TextOverflow::Ellipsis && available_width > 0.0f &&
        !m_lines.empty()) {
        std::string elided = TextOverflowUtils::ellipsize_text_to_width(context, m_lines[0], text_style, letter_spacing,
                                                                        available_width);
        TextMeasurer measurer(context, text_style, letter_spacing);
        float elided_width = measurer.measure(elided);
        m_lines[0] = std::move(elided);
        m_line_widths[0] = elided_width;
        content_width = elided_width;
    }

    m_line_x_offsets.assign(m_lines.size(), 0.0f);

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
    m_line_x_offsets.clear();
    m_line_height = 0.0f;
    m_inline_runs.clear();
}

void TextBox::measure_inline(IGraphicsContext& context) {
    m_inline_runs.clear();
    const auto* style = get_computed_style();
    float letter_spacing = style ? style->letter_spacing : 0.0f;
    m_rendered_text = TextLayoutUtils::build_rendered_text(get_dom_node()->get_text(), style);
    if (style && style->whitespace == Css::ComputedStyle::WhiteSpace::Preserve) {
        layout(context, {0.0f, 0.0f, kInlineMeasurementWidth, 0.0f});
        InlineRun run;
        run.owner = this;
        run.local_index = 0;
        run.text = m_rendered_text;
        run.width = m_rect.width;
        run.height = m_rect.height;
        run.ascent = compute_text_ascent(m_last_metrics, m_line_height);
        run.vertical_align = resolve_inline_vertical_align(style);
        m_inline_runs.push_back(std::move(run));
        return;
    }

    auto tokens = TextLayoutUtils::tokenize_text(m_rendered_text);
    TextStyle text_style = TextStyleUtils::build_text_style(style);
    m_last_metrics = context.measure_text("A", text_style);
    float line_height = TextMetricsUtils::resolve_line_height(style, m_last_metrics.height);
    m_line_height = line_height;
    m_fragments.clear();
    TextMeasurer measurer(context, text_style, letter_spacing);

    bool break_words = style && style->word_wrap == Css::ComputedStyle::WordWrap::BreakWord;
    m_inline_runs.reserve(tokens.size());
    size_t local_index = 0;
    for (const auto& token : tokens) {
        if (break_words && token != " ") {
            size_t idx = 0;
            while (idx < token.size()) {
                size_t next = Core::Utils::next_codepoint(token, idx);
                if (next <= idx) {
                    break;
                }
                std::string glyph = token.substr(idx, next - idx);
                TextMetrics metrics = context.measure_text(glyph, text_style);
                InlineRun run;
                run.owner = this;
                run.local_index = local_index++;
                run.text = std::move(glyph);
                run.width = run.text.empty() ? 0.0f : metrics.width + letter_spacing;
                run.height = line_height;
                run.ascent = compute_text_ascent(metrics, line_height);
                run.vertical_align = resolve_inline_vertical_align(style);
                m_inline_runs.push_back(std::move(run));
                idx = next;
            }
            continue;
        }

        TextMetrics metrics = context.measure_text(token, text_style);
        InlineRun run;
        run.owner = this;
        run.local_index = local_index++;
        run.text = token;
        run.width = measurer.measure(token);
        run.height = line_height;
        run.ascent = compute_text_ascent(metrics, line_height);
        run.vertical_align = resolve_inline_vertical_align(style);
        m_inline_runs.push_back(std::move(run));
    }
    m_fragments.resize(m_inline_runs.size());
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
    m_fragments[index].ascent = fragment.ascent;
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

    if (!m_fragments.empty()) {
        float line_height = m_line_height > 0.0f ? m_line_height : m_last_metrics.height;
        paint_fragments(context, text_style, absolute_x, absolute_y, style, style && style->underline);
        return;
    }

    if (m_lines.empty()) return;

    paint_lines(context, text_style, absolute_x, absolute_y, style, style && style->underline);
}

void TextBox::paint_fragments(IGraphicsContext& context, const TextStyle& text_style, float absolute_x,
                              float absolute_y, const Css::ComputedStyle* style, bool underline) const {
    TextMetrics line_metrics = m_last_metrics;
    if (line_metrics.height <= 0.0f) {
        line_metrics = context.measure_text("A", text_style);
    }
    UnderlineMetrics underline_metrics = resolve_underline_metrics(line_metrics, style);
    float letter_spacing = style ? style->letter_spacing : 0.0f;

    struct LineInfo {
        float max_right = 0.0f;
        float min_y = 0.0f;
        float max_y = 0.0f;
        float baseline = 0.0f;
        bool initialized = false;
    };
    std::vector<LineInfo> lines;
    for (const auto& frag : m_fragments) {
        size_t line_index = frag.line_index;
        if (line_index >= lines.size()) {
            lines.resize(line_index + 1);
        }
        auto& info = lines[line_index];
        float line_right = frag.rect.x + frag.rect.width;
        info.max_right = std::max(info.max_right, line_right);
        float frag_top = frag.rect.y;
        float frag_bottom = frag.rect.y + frag.rect.height;
        if (!info.initialized) {
            info.min_y = frag_top;
            info.max_y = frag_bottom;
            info.baseline = frag.rect.y + frag.ascent;
            info.initialized = true;
        } else {
            info.min_y = std::min(info.min_y, frag_top);
            info.max_y = std::max(info.max_y, frag_bottom);
            info.baseline = std::max(info.baseline, frag.rect.y + frag.ascent);
        }
        draw_spaced_text(context, frag.text, absolute_x + frag.rect.x, absolute_y + frag.rect.y, text_style,
                         letter_spacing);
    }

    if (!underline) {
        return;
    }

    for (const auto& info : lines) {
        if (!info.initialized || info.max_right <= 0.0f) {
            continue;
        }
        float baseline_y = absolute_y + info.baseline;
        float underline_y = baseline_y + underline_metrics.position;
        Hummingbird::Layout::Rect line_rect{absolute_x, underline_y, info.max_right, underline_metrics.thickness};
        context.fill_rect(line_rect, text_style.color);
    }
}

void TextBox::paint_lines(IGraphicsContext& context, const TextStyle& text_style, float absolute_x, float absolute_y,
                          const Css::ComputedStyle* style, bool underline) const {
    float line_height = m_line_height > 0.0f ? m_line_height : m_last_metrics.height;
    TextMetrics line_metrics = m_last_metrics;
    if (line_metrics.height <= 0.0f) {
        line_metrics = context.measure_text("A", text_style);
    }
    UnderlineMetrics underline_metrics = resolve_underline_metrics(line_metrics, style);
    float letter_spacing = style ? style->letter_spacing : 0.0f;
    for (size_t i = 0; i < m_lines.size(); ++i) {
        float line_top = absolute_y + static_cast<float>(i) * line_height;
        float line_x = absolute_x;
        if (i < m_line_x_offsets.size()) {
            line_x += m_line_x_offsets[i];
        }
        float line_width = 0.0f;
        if (i < m_line_widths.size()) {
            line_width = m_line_widths[i];
        }
        if (!m_lines[i].empty()) {
            draw_spaced_text(context, m_lines[i], line_x, line_top, text_style, letter_spacing);
        }
        if (underline && line_width > 0.0f) {
            float underline_y = compute_underline_y(line_top, line_height, line_metrics, underline_metrics);
            Hummingbird::Layout::Rect line_rect{line_x, underline_y, line_width, underline_metrics.thickness};
            context.fill_rect(line_rect, text_style.color);
        }
    }
}

}  // namespace Hummingbird::Layout
