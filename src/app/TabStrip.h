#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "core/platform_api/IGraphicsContext.h"
#include "engine/tab/TabManager.h"

namespace Hummingbird::App {

class TabStrip {
public:
    struct MouseResult {
        bool handled = false;
        bool needs_repaint = false;
        std::optional<Engine::TabId> activated_tab;
    };

    TabStrip();

    int height() const { return height_; }

    void draw(IGraphicsContext& graphics, int win_w, int top_y, const Engine::TabManager& tabs) const;

    MouseResult handle_mouse_down(int x, int y, int win_w, int top_y, const Engine::TabManager& tabs) const;

private:
    std::string label_for_tab(const Engine::TabManager& tabs, Engine::TabId id, int ordinal) const;

    TextStyle style_;
    int height_ = 24;
};

}  // namespace Hummingbird::App
