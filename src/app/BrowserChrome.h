#pragma once

#include <optional>

#include "app/TabStrip.h"
#include "app/UrlBar.h"
#include "core/platform_api/IGraphicsContext.h"
#include "engine/tab/TabManager.h"
#include "layout/geometry/Geometry.h"

namespace Hummingbird {
class IResourceProvider;
class IImageDecoder;
}  // namespace Hummingbird

namespace Hummingbird::App {

class BrowserChrome {
public:
    struct TabStripClickResult {
        bool handled = false;
        bool needs_repaint = false;
        std::optional<Engine::TabId> activated_tab;
    };

    UrlBar& url_bar() { return url_bar_; }
    const UrlBar& url_bar() const { return url_bar_; }

    int tab_strip_height() const { return tab_strip_.height(); }

    // Window area left for document content below the URL bar and tab strip.
    Layout::Rect content_viewport(int win_w, int win_h) const;

    void draw_tab_strip(IGraphicsContext& graphics, int win_w, int top_y, const Engine::TabManager& tabs) const;

    TabStripClickResult handle_tab_strip_mouse_down(int x, int y, int win_w, int top_y,
                                                    const Engine::TabManager& tabs) const;

    void load_security_icons(IResourceProvider* provider, IImageDecoder* decoder);

private:
    UrlBar url_bar_;
    TabStrip tab_strip_;
};

}  // namespace Hummingbird::App
