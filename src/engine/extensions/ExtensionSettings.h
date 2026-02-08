#pragma once

#include <string>
#include <string_view>
#include <unordered_set>

#include "engine/extensions/ExtensionLoader.h"

namespace Hummingbird::Engine {

struct ExtensionSettings {
    // When `enabled_ids` is non-empty, only those extensions are enabled.
    // Otherwise, all extensions are enabled except those listed in `disabled_ids`.
    std::unordered_set<std::string> enabled_ids;
    std::unordered_set<std::string> disabled_ids;
};

// Reads extension settings from environment:
// - HB_EXTENSIONS_ENABLE: comma-separated list of extension IDs to enable (allow-list).
// - HB_EXTENSIONS_DISABLE: comma-separated list of extension IDs to disable (deny-list).
ExtensionSettings extension_settings_from_env();

// An extension ID is currently the extension root directory name.
std::string extension_id(const LoadedExtension& ext);

bool is_extension_enabled(const ExtensionSettings& settings, const LoadedExtension& ext);

}  // namespace Hummingbird::Engine
