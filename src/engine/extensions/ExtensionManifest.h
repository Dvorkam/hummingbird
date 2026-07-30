#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Hummingbird::Engine {

struct ManifestParseError {
    std::string message;
    size_t offset = 0;
};

struct ExtensionManifest {
    std::string name;
    std::string version;
    std::string background_entry;
    std::vector<std::string> permissions;
};

// Parses a minimal manifest JSON object.
// Required:
// - name: string
// - version: string
// - background: { entry: string }
// Optional:
// - permissions: string[]
std::optional<ExtensionManifest> parse_extension_manifest(std::string_view json, ManifestParseError* error);

// Whether `manifest` declares `permission` (story 9.4.1). Comparison is exact:
// permission names are an allow-list, and being lenient about how they are
// spelled is how an allow-list quietly stops being one.
bool manifest_has_permission(const ExtensionManifest& manifest, std::string_view permission);

}  // namespace Hummingbird::Engine
