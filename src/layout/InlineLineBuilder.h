#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "layout/inline/InlineTypes.h"

namespace Hummingbird::Layout {

class InlineLineBuilder {
public:
    void reset();
    void add_run(const InlineRun& run);
    std::vector<InlineLine> layout(float max_width, float start_x = 0.0f);

private:
    struct LayoutCursor {
        float x = 0.0f;
        float y = 0.0f;
        float line_height = 0.0f;
        size_t line_index = 0;
    };

    bool should_wrap(float max_width, const LayoutCursor& cursor, float next_width) const;
    void advance_line(LayoutCursor& cursor);
    InlineFragment build_fragment(size_t run_index, const LayoutCursor& cursor, const InlineRun& run) const;
    std::vector<InlineRun> m_runs;
};

}  // namespace Hummingbird::Layout
