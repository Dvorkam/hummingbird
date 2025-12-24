#include "layout/InlineLineBuilder.h"

#include <algorithm>

namespace Hummingbird::Layout {

void InlineLineBuilder::reset() {
    m_runs.clear();
    m_line_heights.clear();
}

void InlineLineBuilder::add_run(const InlineRun& run) {
    m_runs.push_back(run);
}

bool InlineLineBuilder::should_wrap(float max_width, const LayoutCursor& cursor, float next_width) const {
    return max_width > 0.0f && cursor.x > 0.0f && (cursor.x + next_width) > max_width;
}

void InlineLineBuilder::advance_line(LayoutCursor& cursor) {
    m_line_heights.push_back(cursor.line_height);
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

void InlineLineBuilder::push_line_height(LayoutCursor& cursor, bool has_line) {
    if (has_line || !m_runs.empty()) {
        m_line_heights.push_back(cursor.line_height);
    }
}

std::vector<InlineFragment> InlineLineBuilder::layout(float max_width, float start_x) {
    std::vector<InlineFragment> fragments;
    fragments.reserve(m_runs.size());
    m_line_heights.clear();

    LayoutCursor cursor{start_x, 0.0f, 0.0f, 0};
    bool has_line = false;

    for (size_t i = 0; i < m_runs.size(); ++i) {
        const auto& run = m_runs[i];
        if (should_wrap(max_width, cursor, run.width)) {
            advance_line(cursor);
            has_line = false;
        }

        fragments.push_back(build_fragment(i, cursor, run));

        cursor.line_height = std::max(cursor.line_height, run.height);
        cursor.x += run.width;
        has_line = true;
    }

    push_line_height(cursor, has_line);

    return fragments;
}

}  // namespace Hummingbird::Layout
