#pragma once
#include <memory>

#include "core/IWindow.h"

std::unique_ptr<IWindow> create_window();
