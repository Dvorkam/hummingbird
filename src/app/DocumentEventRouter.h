#pragma once

#include "layout/geometry/Geometry.h"

namespace Hummingbird {
class IGraphicsContext;
class IWindow;
struct InputEvent;
}  // namespace Hummingbird

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
    void handle_mouse_down(const Hummingbird::InputEvent& event);
    void handle_mouse_wheel(const Hummingbird::InputEvent& event);

private:
    bool handle_document_hit_navigation(const Hummingbird::Layout::Point& point,
                                        const Hummingbird::Layout::Rect& viewport);

    BrowserApp& app_;
    BrowserChrome& chrome_;
    RenderCoordinator& render_;
    IWindow* window_ = nullptr;
    IGraphicsContext* graphics_ = nullptr;
};

}  // namespace Hummingbird::App
