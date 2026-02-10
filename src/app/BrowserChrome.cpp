#include "app/BrowserChrome.h"

namespace Hummingbird::App {

void BrowserChrome::draw_tab_strip(IGraphicsContext& graphics, int win_w, int top_y,
                                   const Engine::TabManager& tabs) const {
    tab_strip_.draw(graphics, win_w, top_y, tabs);
}

BrowserChrome::TabStripClickResult BrowserChrome::handle_tab_strip_mouse_down(int x, int y, int win_w, int top_y,
                                                                              const Engine::TabManager& tabs) const {
    auto result = tab_strip_.handle_mouse_down(x, y, win_w, top_y, tabs);
    return {result.handled, result.needs_repaint, result.activated_tab};
}

}  // namespace Hummingbird::App
