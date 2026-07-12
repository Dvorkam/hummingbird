#pragma once

#include <vector>

#include "engine/extensions/ExtensionLoader.h"
#include "engine/extensions/ExtensionSettings.h"

namespace Hummingbird::App {

struct ExtensionBootstrapResult {
    std::vector<Hummingbird::Engine::LoadedExtension> extensions;
    Hummingbird::Engine::ExtensionSettings settings;
    std::vector<Hummingbird::Engine::ExtensionLoadError> errors;
};

ExtensionBootstrapResult load_extension_bootstrap();

}  // namespace Hummingbird::App
