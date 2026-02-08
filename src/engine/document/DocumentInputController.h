#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "core/platform_api/IGraphicsContext.h"
#include "core/platform_api/InputEvent.h"
#include "layout/geometry/Geometry.h"

namespace Hummingbird::DOM {
class Element;
}

namespace Hummingbird::Layout {
class RenderObject;
}

namespace Hummingbird::Engine {

class DocumentInputController {
public:
    struct EditResult {
        bool handled = false;
        bool needs_repaint = false;
    };

    void reset();
    bool has_focus() const { return focused_input_ != nullptr; }
    const DOM::Element* focused_element() const { return focused_input_; }
    bool focus_input_at(const Layout::RenderObject* render_tree, const Layout::Point& point,
                        const Layout::Rect& viewport, float scroll_y);
    bool focus_autofocus_input(const Layout::RenderObject* render_tree);
    bool clear_focus();

    EditResult handle_text_input(std::string_view text);
    EditResult handle_key_down(const InputEvent& event);
    std::optional<std::string> focused_value() const;

    void paint_controls(const Layout::RenderObject* render_tree, IGraphicsContext& graphics,
                        const Layout::Rect& viewport, float scroll_y, bool repaint_background) const;

private:
    DOM::Element* focused_input_ = nullptr;
    std::string::size_type caret_ = 0;
};

}  // namespace Hummingbird::Engine
