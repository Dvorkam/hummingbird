#pragma once

#include <algorithm>

#include "layout/geometry/Geometry.h"

namespace Hummingbird::Engine {

class TabLayoutState {
public:
    float scroll_y = 0.0f;
    float content_height = 0.0f;
    Layout::Rect last_viewport{0, 0, 0, 0};
    bool has_viewport = false;

    bool viewport_changed(const Layout::Rect& viewport) const {
        return !has_viewport || viewport.x != last_viewport.x || viewport.y != last_viewport.y ||
               viewport.width != last_viewport.width || viewport.height != last_viewport.height;
    }

    void reset() {
        scroll_y = 0.0f;
        content_height = 0.0f;
        has_viewport = false;
        last_viewport = Layout::Rect{0, 0, 0, 0};
    }

    void update(const Layout::Rect& viewport, float new_content_height) {
        content_height = new_content_height;
        last_viewport = viewport;
        has_viewport = true;
        clamp_scroll(viewport.height);
    }

    void clamp_scroll(float viewport_height) {
        const float max_scroll = std::max(0.0f, content_height - viewport_height);
        scroll_y = std::clamp(scroll_y, 0.0f, max_scroll);
    }

    void scroll_by(float delta_px, float viewport_height) {
        scroll_y -= delta_px;
        clamp_scroll(viewport_height);
    }
};

}  // namespace Hummingbird::Engine
