#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "engine/extensions/ExtensionManifest.h"

namespace Hummingbird::Engine {

struct ExtensionLoadError {
    std::string message;
    std::filesystem::path path;
};

struct LoadedExtension {
    ExtensionManifest manifest;
    std::filesystem::path root_dir;
    std::filesystem::path manifest_path;
    std::filesystem::path background_entry_path;
};

// Uses `HB_EXTENSIONS_DIR` if set, otherwise resolves `assets/extensions` via AssetPath.
std::filesystem::path default_extensions_root();

// Discovers extensions as subdirectories under `root_dir` and loads their manifest.
// Returns only successfully loaded extensions; errors (if requested) are appended to `errors`.
std::vector<LoadedExtension> load_extensions_from_root(const std::filesystem::path& root_dir,
                                                       std::vector<ExtensionLoadError>* errors);

}  // namespace Hummingbird::Engine
