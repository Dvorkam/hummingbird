#include "app/BrowserEventRouter.h"

#include "app/BrowserApp.h"

namespace Hummingbird::App {

void BrowserEventRouter::handle_event(const InputEvent& event) {
    switch (event.type) {
        case EventType::Quit:
            app_.handle_quit_event();
            return;
        case EventType::TextInput:
            app_.handle_text_input_event(event);
            return;
        case EventType::KeyDown:
            app_.handle_key_down_event(event);
            return;
        case EventType::MouseDown:
            app_.handle_mouse_down_event(event);
            return;
        case EventType::MouseWheel:
            app_.handle_mouse_wheel_event(event);
            return;
        case EventType::Resize:
            app_.handle_resize_event(event);
            return;
        default:
            return;
    }
}

}  // namespace Hummingbird::App
