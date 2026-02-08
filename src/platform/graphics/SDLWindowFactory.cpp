#include <memory>

#include "core/platform_api/WindowFactory.h"
#include "platform/graphics/SDLWindow.h"

namespace Hummingbird {
class IWindow;

std::unique_ptr<IWindow> create_window() {
    return std::make_unique<Hummingbird::Platform::SDLWindow>();
}

}  // namespace Hummingbird
