#include "layout/flow/InlineLineBuilder.h"

#include <algorithm>
#include <utility>

#include "layout/geometry/Geometry.h"

namespace Hummingbird::Layout {

namespace {
float resolve_run_ascent(const InlineRun& run) {
    if (run.ascent > 0.0f) {
        return std::min(run.ascent, run.height > 0.0f ? run.height : run.ascent);
    }
    return run.height;
}
}  // namespace

void InlineLineBuilder::reset() {
    m_runs.clear();
}

void InlineLineBuilder::add_run(const InlineRun& run) {
    m_runs.push_back(run);
}

bool InlineLineBuilder::should_wrap(float max_width, const LayoutCursor& cursor, float next_width) const {
    return max_width > 0.0f && cursor.x > 0.0f && (cursor.x + next_width) > max_width;
}

void InlineLineBuilder::advance_line(LayoutCursor& cursor) {
    cursor.y += cursor.line_height;
    cursor.x = 0.0f;
    cursor.line_height = 0.0f;
    ++cursor.line_index;
}

InlineFragment InlineLineBuilder::build_fragment(size_t run_index, const LayoutCursor& cursor,
                                                 const InlineRun& run) const {
    InlineFragment fragment;
    fragment.run_index = run_index;
    fragment.line_index = cursor.line_index;
    fragment.rect = {cursor.x, cursor.y, run.width, run.height};
    fragment.ascent = resolve_run_ascent(run);
    return fragment;
}

std::vector<InlineLine> InlineLineBuilder::layout(float max_width, float start_x) {
    std::vector<InlineLine> lines;
    lines.reserve(m_runs.size());

    LayoutCursor cursor{start_x, 0.0f, 0.0f, 0};
    bool has_line = false;
    InlineLine current_line;
    float line_ascent = 0.0f;
    float line_descent = 0.0f;
    float line_top = 0.0f;

    auto finalize_line = [&](InlineLine& line) {
        if (!has_line || line.fragments.empty()) {
            return;
        }
        line.ascent = line_ascent;
        line.descent = line_descent;
        line.height = line_ascent + line_descent;
        for (auto& fragment : line.fragments) {
            const auto& run = m_runs[fragment.run_index];
            float run_ascent = resolve_run_ascent(run);
            switch (run.vertical_align) {
                case InlineVerticalAlign::Top:
                    fragment.rect.y = line_top;
                    break;
                case InlineVerticalAlign::Middle:
                    fragment.rect.y = line_top + std::max(0.0f, (line.height - run.height) * 0.5f);
                    break;
                case InlineVerticalAlign::Bottom:
                    fragment.rect.y = line_top + std::max(0.0f, line.height - run.height);
                    break;
                case InlineVerticalAlign::Baseline:
                default:
                    fragment.rect.y = line_top + (line_ascent - run_ascent);
                    break;
            }
        }
        cursor.line_height = line.height;
    };

    for (size_t i = 0; i < m_runs.size(); ++i) {
        const auto& run = m_runs[i];
        if (should_wrap(max_width, cursor, run.width)) {
            if (has_line) {
                finalize_line(current_line);
                lines.push_back(std::move(current_line));
                current_line = InlineLine{};
                advance_line(cursor);
                has_line = false;
            }
        }

        if (!has_line) {
            current_line = InlineLine{};
            line_ascent = 0.0f;
            line_descent = 0.0f;
            line_top = cursor.y;
            has_line = true;
        }
        current_line.fragments.push_back(build_fragment(i, cursor, run));

        float run_ascent = resolve_run_ascent(run);
        float run_descent = run.height - run_ascent;
        if (run_descent < 0.0f) {
            run_descent = 0.0f;
        }
        line_ascent = std::max(line_ascent, run_ascent);
        line_descent = std::max(line_descent, run_descent);
        cursor.line_height = std::max(cursor.line_height, line_ascent + line_descent);
        cursor.x += run.width;
    }

    if (has_line || !m_runs.empty()) {
        finalize_line(current_line);
        lines.push_back(std::move(current_line));
    }

    return lines;
}

}  // namespace Hummingbird::Layout
