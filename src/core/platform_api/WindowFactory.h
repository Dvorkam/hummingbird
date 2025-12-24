#pragma once
#include <memory>

#include "core/platform_api/IWindow.h"

std::unique_ptr<IWindow> create_window();
