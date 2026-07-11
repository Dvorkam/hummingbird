#pragma once

namespace Hummingbird {
class IGraphicsContext;
class IWindow;
struct InputEvent;
}  // namespace Hummingbird

namespace Hummingbird::App {

class BrowserApp;
class BrowserChrome;
class RenderCoordinator;
class TabController;

class ChromeEventRouter {
public:
    ChromeEventRouter(BrowserApp& app, BrowserChrome& chrome, TabController& tabs, RenderCoordinator& render,
                      IWindow* window, IGraphicsContext* graphics)
        : app_(app), chrome_(chrome), tabs_(tabs), render_(render), window_(window), graphics_(graphics) {}

    bool handle_text_input(const Hummingbird::InputEvent& event);
    bool handle_key_down(const Hummingbird::InputEvent& event);
    bool handle_mouse_down(const Hummingbird::InputEvent& event);

private:
    bool handle_tab_shortcut(const Hummingbird::InputEvent& event);
    bool handle_global_key_shortcut(const Hummingbird::InputEvent& event);
    bool handle_url_bar_key_down(const Hummingbird::InputEvent& event);
    bool handle_tab_strip_mouse_down(const Hummingbird::InputEvent& event);
    bool handle_url_bar_mouse_down(const Hummingbird::InputEvent& event);

    BrowserApp& app_;
    BrowserChrome& chrome_;
    TabController& tabs_;
    RenderCoordinator& render_;
    IWindow* window_ = nullptr;
    IGraphicsContext* graphics_ = nullptr;
};

}  // namespace Hummingbird::App
