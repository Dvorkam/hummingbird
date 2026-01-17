#include "core/platform_api/WindowFactory.h"
#include "platform/SDLWindow.h"

namespace Hummingbird {

std::unique_ptr<IWindow> create_window() {
    return std::make_unique<Hummingbird::Platform::SDLWindow>();
}

}  // namespace Hummingbird
