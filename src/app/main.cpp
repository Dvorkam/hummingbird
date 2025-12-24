#include <memory>

#include "app/BrowserApp.h"
#include "core/WindowFactory.h"
#include "core/platform_api/IWindow.h"

int main(int /*argc*/, char* /*argv*/[]) {
    auto window = create_window();
    window->open();

    if (!window->is_open()) return 1;

    auto gfx = window->get_graphics_context();
    if (!gfx) return 1;

    BrowserApp app(std::move(window));
    app.start();  // initial navigation + initial UI focus

    while (app.tick()) {  // one “frame”
        // nothing here
    }

    return 0;
}
