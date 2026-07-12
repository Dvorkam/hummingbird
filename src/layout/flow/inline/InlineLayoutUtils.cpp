#include "layout/flow/inline/InlineLayoutUtils.h"

#include <algorithm>

#include "layout/RenderObject.h"
#include "layout/flow/InlineLineBuilder.h"
#include "layout/flow/TextStyleUtils.h"
#include "layout/flow/inline/IInlineParticipant.h"
#include "layout/flow/inline/InlineRef.h"
#include "layout/geometry/Geometry.h"

namespace Hummingbird {
class IGraphicsContext;
}  // namespace Hummingbird

namespace Hummingbird::Layout::InlineLayout {

namespace {

float measure_text_width(IGraphicsContext& context, std::string_view text, const TextStyle& style,
                         float letter_spacing) {
    if (text.empty()) {
        return 0.0f;
    }
    TextMetrics metrics = context.measure_text(std::string(text), style);
    if (letter_spacing == 0.0f) {
        return metrics.width;
    }
    size_t chars = text.size();
    if (chars > 1) {
        return metrics.width + letter_spacing * static_cast<float>(chars - 1);
    }
    return metrics.width;
}

std::string ellipsize_inline_text(IGraphicsContext& context, std::string_view text, const TextStyle& style,
                                  float letter_spacing, float max_width) {
    constexpr std::string_view kEllipsis = "...";
    if (max_width <= 0.0f) {
        return "";
    }
    float ellipsis_width = measure_text_width(context, kEllipsis, style, letter_spacing);
    if (ellipsis_width >= max_width) {
        return std::string(kEllipsis);
    }

    std::string out;
    for (size_t i = 0; i < text.size(); ++i) {
        std::string candidate = out + text[i] + std::string(kEllipsis);
        if (measure_text_width(context, candidate, style, letter_spacing) > max_width) {
            break;
        }
        out.push_back(text[i]);
    }
    out += std::string(kEllipsis);
    return out;
}

void apply_ellipsis_to_first_line(IGraphicsContext& context, std::vector<InlineLine>& lines,
                                  std::vector<InlineRun>& runs, float content_width) {
    if (lines.empty() || content_width <= 0.0f) {
        return;
    }
    auto& line = lines.front();
    if (line.fragments.empty()) {
        return;
    }
    float used = 0.0f;
    std::vector<InlineFragment> kept;
    kept.reserve(line.fragments.size());

    for (const auto& fragment : line.fragments) {
        const InlineRun& run = runs[fragment.run_index];
        if (used + fragment.rect.width <= content_width) {
            kept.push_back(fragment);
            used += fragment.rect.width;
            continue;
        }

        if (fragment.run_index < runs.size()) {
            InlineRun& mutable_run = runs[fragment.run_index];
            const auto* run_style = mutable_run.owner ? mutable_run.owner->get_computed_style() : nullptr;
            TextStyle text_style = TextStyleUtils::build_text_style(run_style);
            float letter_spacing = run_style ? run_style->letter_spacing : 0.0f;
            std::string truncated =
                ellipsize_inline_text(context, mutable_run.text, text_style, letter_spacing, content_width - used);
            float truncated_width = measure_text_width(context, truncated, text_style, letter_spacing);
            if (!truncated.empty() && truncated_width > 0.0f) {
                mutable_run.text = truncated;
                mutable_run.width = truncated_width;
                InlineFragment truncated_fragment = fragment;
                truncated_fragment.rect.width = truncated_width;
                kept.push_back(truncated_fragment);
            }
        }
        break;
    }

    line.fragments = std::move(kept);
}

}  // namespace

void measure_inline_participants(IGraphicsContext& context, std::vector<std::unique_ptr<RenderObject>>& children,
                                 size_t& i) {
    while (i < children.size()) {
        auto inl = children[i]->Inline();
        if (!inl) break;

        inl.get().reset_inline_layout();
        inl.get().measure_inline(context);
        ++i;
    }
}

void collect_inline_runs(IGraphicsContext& context, std::vector<std::unique_ptr<RenderObject>>& children, size_t& i,
                         std::vector<InlineRun>& runs) {
    while (i < children.size()) {
        auto inl = children[i]->Inline();
        if (!inl) break;

        inl.get().collect_inline_runs(context, runs);
        ++i;
    }
}

InlineLayoutResult layout_inline_group(IGraphicsContext& context, std::vector<std::unique_ptr<RenderObject>>& children,
                                       size_t& i, const GroupLayoutContext& layout) {
    InlineLayoutResult result;
    InlineLineBuilder builder;
    std::vector<InlineRun> runs;
    size_t group_start = i;
    size_t group_end = i;

    measure_inline_participants(context, children, group_end);
    i = group_start;
    collect_inline_runs(context, children, i, runs);

    if (runs.empty()) {
        return result;
    }

    for (const auto& run : runs) {
        builder.add_run(run);
    }

    auto lines = builder.layout(layout.wrap_width, layout.start_x);
    if (lines.empty()) {
        return result;
    }

    if (layout.no_wrap && layout.text_overflow_ellipsis) {
        apply_ellipsis_to_first_line(context, lines, runs, layout.content_width);
    }

    align_inline_lines(lines, layout.content_width, layout.align);
    result = apply_inline_fragments(lines, runs, layout.base_x, layout.base_y, layout.capture_fragments);

    for (size_t j = group_start; j < group_end; ++j) {
        if (auto inl = children[j]->Inline()) {
            inl.get().finalize_inline_layout();
        }
    }

    return result;
}

void align_inline_lines(std::vector<InlineLine>& lines, float available_width, Css::ComputedStyle::TextAlign align) {
    if (align == Css::ComputedStyle::TextAlign::Left || available_width <= 0.0f) {
        return;
    }

    for (auto& line : lines) {
        if (line.fragments.empty()) {
            continue;
        }
        float min_x = line.fragments.front().rect.x;
        float max_x = line.fragments.front().rect.x + line.fragments.front().rect.width;
        for (const auto& fragment : line.fragments) {
            min_x = std::min(min_x, fragment.rect.x);
            max_x = std::max(max_x, fragment.rect.x + fragment.rect.width);
        }
        float line_width = max_x - min_x;
        if (line_width <= 0.0f || line_width >= available_width) {
            continue;
        }

        float desired_start = 0.0f;
        if (align == Css::ComputedStyle::TextAlign::Center) {
            desired_start = (available_width - line_width) * 0.5f;
        } else if (align == Css::ComputedStyle::TextAlign::Right) {
            desired_start = available_width - line_width;
        }
        float shift = desired_start - min_x;
        if (shift == 0.0f) {
            continue;
        }
        for (auto& fragment : line.fragments) {
            fragment.rect.x += shift;
        }
    }
}

InlineLayoutResult apply_inline_fragments(const std::vector<InlineLine>& lines, const std::vector<InlineRun>& runs,
                                          float base_x, float base_y, bool capture_fragments) {
    InlineLayoutResult result;
    if (lines.empty()) {
        return result;
    }

    result.heights.reserve(lines.size());
    if (capture_fragments) {
        result.fragments.reserve(runs.size());
    }
    size_t last_line = lines.size() - 1;

    for (const auto& line : lines) {
        result.heights.push_back(line.height);
        for (const auto& fragment : line.fragments) {
            InlineFragment resolved = fragment;
            resolved.rect.x += base_x;
            resolved.rect.y += base_y;
            if (capture_fragments) {
                result.fragments.push_back(resolved);
            }
            auto& run = runs[resolved.run_index];
            if (run.owner) {
                if (auto inl = run.owner->Inline()) {
                    inl.get().apply_inline_fragment(run.local_index, resolved, run);
                }
            }
            if (resolved.line_index == last_line) {
                float extent = resolved.rect.x + resolved.rect.width - base_x;
                result.last_line_width = std::max(result.last_line_width, extent);
            }
        }
    }

    return result;
}

void update_cursor_for_inline(float& cursor_x, float& cursor_y, float& cursor_line_height, float base_x, float base_y,
                              const InlineLayoutResult& layout) {
    if (layout.heights.empty()) {
        return;
    }

    float total_height = 0.0f;
    for (float h : layout.heights) {
        total_height += h;
    }
    float last_height = layout.heights.back();
    cursor_y = base_y + (total_height - last_height);
    cursor_x = base_x + layout.last_line_width;
    cursor_line_height = std::max(cursor_line_height, last_height);
}

}  // namespace Hummingbird::Layout::InlineLayout
