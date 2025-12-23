#include "core/WindowFactory.h"
#include "platform/SDLWindow.h"

std::unique_ptr<IWindow> create_window() {
    return std::make_unique<SDLWindow>();
}
