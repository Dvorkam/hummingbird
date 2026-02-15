#pragma once

namespace Hummingbird {
struct InputEvent;
}

namespace Hummingbird::App {

class BrowserApp;

class ChromeEventRouter {
public:
    explicit ChromeEventRouter(BrowserApp& app) : app_(app) {}
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
};

}  // namespace Hummingbird::App
