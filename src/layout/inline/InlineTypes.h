#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "layout/Geometry.h"

namespace Hummingbird::Layout {

class RenderObject;  // fwd

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

struct InlineLine {
    float height = 0.0f;
    std::vector<InlineFragment> fragments;
};

}  // namespace Hummingbird::Layout
