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

std::vector<InlineFragment> InlineLineBuilder::layout(float max_width, float start_x) {
    std::vector<InlineFragment> fragments;
    fragments.reserve(m_runs.size());
    m_line_heights.clear();

    float cursor_x = start_x;
    float cursor_y = 0.0f;
    float line_height = 0.0f;
    size_t line_index = 0;
    bool has_line = false;

    for (size_t i = 0; i < m_runs.size(); ++i) {
        const auto& run = m_runs[i];
        bool should_wrap = max_width > 0.0f && cursor_x > 0.0f && (cursor_x + run.width) > max_width;
        if (should_wrap) {
            m_line_heights.push_back(line_height);
            cursor_y += line_height;
            cursor_x = 0.0f;
            line_height = 0.0f;
            ++line_index;
            has_line = false;
        }

        InlineFragment fragment;
        fragment.run_index = i;
        fragment.line_index = line_index;
        fragment.rect = {cursor_x, cursor_y, run.width, run.height};
        fragments.push_back(fragment);

        line_height = std::max(line_height, run.height);
        cursor_x += run.width;
        has_line = true;
    }

    if (has_line || !m_runs.empty()) {
        m_line_heights.push_back(line_height);
    }

    return fragments;
}

}  // namespace Hummingbird::Layout
