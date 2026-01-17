#include "layout/inline/InlineLayoutUtils.h"

#include <algorithm>

#include "layout/InlineLineBuilder.h"
#include "layout/Geometry.h"
#include "layout/RenderObject.h"
#include "layout/inline/IInlineParticipant.h"
#include "layout/inline/InlineRef.h"

namespace Hummingbird {
class IGraphicsContext;
}  // namespace Hummingbird

namespace Hummingbird::Layout::InlineLayout {

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
