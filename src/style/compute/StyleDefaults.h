#pragma once

#include "style/types/ComputedStyle.h"

namespace Hummingbird {
namespace Css {
struct ComputedStyle;
}  // namespace Css
}  // namespace Hummingbird

namespace Hummingbird::DOM {
class Element;
}

namespace Hummingbird::Css::StyleDefaults {

struct StyleOverrides {
    bool color = false;
    bool underline = false;
    bool underline_thickness = false;
    bool underline_offset = false;
    bool link_color = false;
    bool vlink_color = false;
    bool whitespace = false;
    bool font_monospace = false;
    bool weight = false;
    bool style = false;
    bool font_size = false;
    bool font_face = false;
    bool text_align = false;
    bool text_transform = false;
    bool letter_spacing = false;
    bool text_indent = false;
    bool word_wrap = false;
    bool background = false;
    bool line_height = false;
    bool list_style_type = false;
    bool list_style_position = false;
    bool cursor = false;
    bool visibility = false;
    bool pointer_events = false;
};

void apply_user_agent_defaults(const DOM::Element& element, ComputedStyle& style, StyleOverrides& overrides,
                               bool display_set, const ComputedStyle* parent_style);
void apply_legacy_attributes(const DOM::Element& element, ComputedStyle& style, StyleOverrides& overrides);

}  // namespace Hummingbird::Css::StyleDefaults
