#pragma once

#include "core/platform_api/InputEvent.h"

namespace Hummingbird::App {

class BrowserApp;

class BrowserEventRouter {
public:
    explicit BrowserEventRouter(BrowserApp& app) : app_(app) {}

    void handle_event(const InputEvent& event);

private:
    BrowserApp& app_;
};

}  // namespace Hummingbird::App
