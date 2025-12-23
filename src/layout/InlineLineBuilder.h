#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "layout/RenderObject.h"

namespace Hummingbird::Layout {

struct InlineRun {
    RenderObject* owner = nullptr;
    size_t local_index = 0;
    std::string text;
    float width = 0.0f;
    float height = 0.0f;
};

struct InlineFragment {
    size_t run_index = 0;
    size_t line_index = 0;
    Rect rect;
};

class InlineLineBuilder {
public:
    void reset();
    void add_run(const InlineRun& run);
    std::vector<InlineFragment> layout(float max_width, float start_x = 0.0f);
    const std::vector<float>& line_heights() const { return m_line_heights; }

private:
    std::vector<InlineRun> m_runs;
    std::vector<float> m_line_heights;
};

}  // namespace Hummingbird::Layout
