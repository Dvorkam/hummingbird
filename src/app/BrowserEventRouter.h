#pragma once

#include "core/platform_api/InputEvent.h"

namespace Hummingbird::App {

class BrowserApp;
class ChromeEventRouter;
class DocumentEventRouter;
class RenderCoordinator;

// Routes platform input events: chrome (URL bar, tab strip, shortcuts) gets the
// first chance to consume an event; unconsumed events fall through to the document.
class BrowserEventRouter {
public:
    BrowserEventRouter(BrowserApp& app, ChromeEventRouter& chrome, DocumentEventRouter& document,
                       RenderCoordinator& render)
        : app_(app), chrome_(chrome), document_(document), render_(render) {}

    void handle_event(const InputEvent& event);

private:
    BrowserApp& app_;
    ChromeEventRouter& chrome_;
    DocumentEventRouter& document_;
    RenderCoordinator& render_;
};

}  // namespace Hummingbird::App
