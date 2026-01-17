#pragma once

#include <memory>
#include <vector>

#include "core/platform_api/IGraphicsContext.h"
#include "layout/RenderObject.h"
#include "layout/inline/InlineTypes.h"
#include "style/ComputedStyle.h"

namespace Hummingbird::Layout::InlineLayout {

struct InlineLayoutResult {
    std::vector<InlineFragment> fragments;
    std::vector<float> heights;
    float last_line_width = 0.0f;
};

struct GroupLayoutContext {
    float start_x = 0.0f;
    float base_x = 0.0f;
    float base_y = 0.0f;
    float content_width = 0.0f;
    Css::ComputedStyle::TextAlign align = Css::ComputedStyle::TextAlign::Left;
    float wrap_width = 0.0f;
    bool capture_fragments = false;
};

void measure_inline_participants(IGraphicsContext& context, std::vector<std::unique_ptr<RenderObject>>& children,
                                 size_t& i);
void collect_inline_runs(IGraphicsContext& context, std::vector<std::unique_ptr<RenderObject>>& children, size_t& i,
                         std::vector<InlineRun>& runs);
InlineLayoutResult layout_inline_group(IGraphicsContext& context, std::vector<std::unique_ptr<RenderObject>>& children,
                                       size_t& i, const GroupLayoutContext& layout);
void align_inline_lines(std::vector<InlineLine>& lines, float available_width, Css::ComputedStyle::TextAlign align);
InlineLayoutResult apply_inline_fragments(const std::vector<InlineLine>& lines, const std::vector<InlineRun>& runs,
                                          float base_x, float base_y, bool capture_fragments);
void update_cursor_for_inline(float& cursor_x, float& cursor_y, float& cursor_line_height, float base_x, float base_y,
                              const InlineLayoutResult& layout);

}  // namespace Hummingbird::Layout::InlineLayout
