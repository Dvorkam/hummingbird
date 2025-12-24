#include "layout/InlineLineBuilder.h"

#include <algorithm>

namespace Hummingbird::Layout {

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
    return fragment;
}

std::vector<InlineLine> InlineLineBuilder::layout(float max_width, float start_x) {
    std::vector<InlineLine> lines;
    lines.reserve(m_runs.size());

    LayoutCursor cursor{start_x, 0.0f, 0.0f, 0};
    bool has_line = false;
    InlineLine current_line;

    for (size_t i = 0; i < m_runs.size(); ++i) {
        const auto& run = m_runs[i];
        if (should_wrap(max_width, cursor, run.width)) {
            if (has_line) {
                current_line.height = cursor.line_height;
                lines.push_back(std::move(current_line));
                current_line = InlineLine{};
                advance_line(cursor);
                has_line = false;
            }
        }

        if (!has_line) {
            current_line = InlineLine{};
            has_line = true;
        }
        current_line.fragments.push_back(build_fragment(i, cursor, run));

        cursor.line_height = std::max(cursor.line_height, run.height);
        cursor.x += run.width;
    }

    if (has_line || !m_runs.empty()) {
        current_line.height = cursor.line_height;
        lines.push_back(std::move(current_line));
    }

    return lines;
}

}  // namespace Hummingbird::Layout
