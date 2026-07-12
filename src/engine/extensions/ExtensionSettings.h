#pragma once

#include <filesystem>
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

// Parses a simple INI text. Supported section/key format:
// [extensions]
// dark-mode = enabled|disabled
ExtensionSettings extension_settings_from_ini(std::string_view ini_text);

// Loads extension settings from an INI file. Missing/unreadable files return defaults.
ExtensionSettings extension_settings_from_ini_file(const std::filesystem::path& ini_path);

// Returns INI settings path:
// - HB_SETTINGS_INI, when set
// - otherwise assets/config/browser.ini (resolved via asset path)
std::filesystem::path default_extension_settings_ini_path();

// Applies `overrides` on top of `base`.
ExtensionSettings merge_extension_settings(ExtensionSettings base, const ExtensionSettings& overrides);

// An extension ID is currently the extension root directory name.
std::string extension_id(const LoadedExtension& ext);

bool is_extension_enabled(const ExtensionSettings& settings, const LoadedExtension& ext);

}  // namespace Hummingbird::Engine
