#pragma once

#include "layout/geometry/Geometry.h"

namespace Hummingbird {
struct InputEvent;
}

namespace Hummingbird::App {

class BrowserApp;

class DocumentEventRouter {
public:
    explicit DocumentEventRouter(BrowserApp& app) : app_(app) {}
    bool handle_text_input(const Hummingbird::InputEvent& event);
    bool handle_key_down(const Hummingbird::InputEvent& event);
    void handle_mouse_down(const Hummingbird::InputEvent& event);
    void handle_mouse_wheel(const Hummingbird::InputEvent& event);

private:
    bool handle_document_hit_navigation(const Hummingbird::Layout::Point& point,
                                        const Hummingbird::Layout::Rect& viewport);

    BrowserApp& app_;
};

}  // namespace Hummingbird::App
