#pragma once

#include <stddef.h>

#include <memory>
#include <vector>

#include "core/platform_api/IGraphicsContext.h"
#include "layout/RenderObject.h"
#include "layout/block/FloatLayoutUtils.h"
#include "layout/flow/inline/InlineLayoutUtils.h"
#include "style/compute/ComputedStyle.h"

namespace Hummingbird {
class IGraphicsContext;
namespace Layout {
class RenderObject;
}  // namespace Layout
}  // namespace Hummingbird

namespace Hummingbird::Layout::FlowLayout {

struct LineCursor {
    float x = 0.0f;
    float y = 0.0f;
    float line_height = 0.0f;
};

struct ChildMargins {
    float left = 0.0f;
    float right = 0.0f;
    float top = 0.0f;
    float bottom = 0.0f;
    bool left_auto = false;
    bool right_auto = false;
};

ChildMargins compute_child_margins(const Css::ComputedStyle* style, bool allow_auto);
Css::ComputedStyle::Float resolve_float_type(RenderObject& child, bool ignore_absolute);
void flush_line(LineCursor& cursor, float cursor_left);

void layout_block_child(IGraphicsContext& context, RenderObject& child, const ChildMargins& margins, LineCursor& cursor,
                        float content_left, float content_width);
void layout_float_child(IGraphicsContext& context, RenderObject& child, const ChildMargins& margins, LineCursor& cursor,
                        Css::ComputedStyle::Float float_type, std::vector<FloatLayout::FloatBox>& floats,
                        float& max_float_bottom, float content_left, float content_right);

InlineLayout::InlineLayoutResult layout_inline_group(IGraphicsContext& context,
                                                     std::vector<std::unique_ptr<RenderObject>>& children, size_t& i,
                                                     LineCursor& cursor, float base_x, float content_width,
                                                     Css::ComputedStyle::TextAlign text_align, float wrap_width,
                                                     bool capture_fragments);

}  // namespace Hummingbird::Layout::FlowLayout
