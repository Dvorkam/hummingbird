#pragma once
#include <memory>

#include "core/platform_api/IWindow.h"

namespace Hummingbird {

std::unique_ptr<IWindow> create_window();

}  // namespace Hummingbird
