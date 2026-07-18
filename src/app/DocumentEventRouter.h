#pragma once

#include "layout/geometry/Geometry.h"

namespace Hummingbird {
class IGraphicsContext;
class IWindow;
struct InputEvent;
}  // namespace Hummingbird

namespace Hummingbird::Engine {
struct FormSubmission;
}

namespace Hummingbird::App {

class BrowserApp;
class BrowserChrome;
class RenderCoordinator;

class DocumentEventRouter {
public:
    DocumentEventRouter(BrowserApp& app, BrowserChrome& chrome, RenderCoordinator& render, IWindow* window,
                        IGraphicsContext* graphics)
        : app_(app), chrome_(chrome), render_(render), window_(window), graphics_(graphics) {}

    bool handle_text_input(const Hummingbird::InputEvent& event);
    bool handle_key_down(const Hummingbird::InputEvent& event);
    bool handle_key_up(const Hummingbird::InputEvent& event);
    void handle_mouse_down(const Hummingbird::InputEvent& event);
    void handle_mouse_wheel(const Hummingbird::InputEvent& event);

private:
    bool handle_document_hit_navigation(const Hummingbird::Layout::Point& point,
                                        const Hummingbird::Layout::Rect& viewport);
    // Rebuilds + repaints the document after a JS event listener mutated the DOM.
    void rebuild_after_script_mutation();
    // Fires the DOM `submit` event; navigates only if no listener called
    // preventDefault. Returns true if a navigation was started.
    bool submit_or_navigate(const Hummingbird::Engine::FormSubmission& submission);

    BrowserApp& app_;
    BrowserChrome& chrome_;
    RenderCoordinator& render_;
    IWindow* window_ = nullptr;
    IGraphicsContext* graphics_ = nullptr;
};

}  // namespace Hummingbird::App
