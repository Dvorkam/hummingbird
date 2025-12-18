#include "platform/SDLWindow.h"
#include "core/IGraphicsContext.h"
#include <memory>

int main(int argc, char* argv[]) {
    // Our "main" function for the browser engine will likely evolve to handle
    // command-line arguments, configuration, etc. For now, it just creates a window.
    auto window = std::make_unique<SDLWindow>();
    window->open();

    if (!window->is_open()) {
        return 1; // Exit if window creation failed
    }

    auto graphics = window->get_graphics_context();
    const Color teal = {0, 128, 128, 255};

    // This is the main event loop of our application.
    // It will handle user input, rendering, and other updates.
    while (window->is_open()) {
        // Handle events (like closing the window)
        window->update();

        // Render the scene
        graphics->clear(teal);

        // Show the rendered frame
        graphics->present();
    }

    // The window unique_ptr will automatically call the destructor,
    // which handles cleanup. Explicitly calling close is also fine.
    window->close();

    return 0;
}