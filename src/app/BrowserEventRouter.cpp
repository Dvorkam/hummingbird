#include "app/BrowserEventRouter.h"

#include "app/BrowserApp.h"
#include "app/ChromeEventRouter.h"
#include "app/DocumentEventRouter.h"
#include "app/RenderCoordinator.h"

namespace Hummingbird::App {

void BrowserEventRouter::handle_event(const InputEvent& event) {
    switch (event.type) {
        case EventType::Quit:
            app_.shutdown();
            return;
        case EventType::TextInput:
            if (!chrome_.handle_text_input(event)) {
                (void)document_.handle_text_input(event);
            }
            return;
        case EventType::KeyDown:
            if (!chrome_.handle_key_down(event)) {
                (void)document_.handle_key_down(event);
            }
            return;
        case EventType::KeyUp:
            (void)document_.handle_key_up(event);
            return;
        case EventType::MouseDown:
            if (!chrome_.handle_mouse_down(event)) {
                document_.handle_mouse_down(event);
            }
            return;
        case EventType::MouseWheel:
            document_.handle_mouse_wheel(event);
            return;
        case EventType::Resize:
            render_.invalidate_document_cache();
            return;
        default:
            return;
    }
}

}  // namespace Hummingbird::App
