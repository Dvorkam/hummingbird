#include "app/ExtensionBootstrap.h"

namespace Hummingbird::App {

ExtensionBootstrapResult load_extension_bootstrap() {
    ExtensionBootstrapResult result;
    result.extensions = Hummingbird::Engine::load_extensions_from_root(
        Hummingbird::Engine::default_extensions_root(), &result.errors);

    auto ini_settings = Hummingbird::Engine::extension_settings_from_ini_file(
        Hummingbird::Engine::default_extension_settings_ini_path());
    auto env_settings = Hummingbird::Engine::extension_settings_from_env();
    result.settings = Hummingbird::Engine::merge_extension_settings(std::move(ini_settings), env_settings);

    return result;
}

}  // namespace Hummingbird::App
