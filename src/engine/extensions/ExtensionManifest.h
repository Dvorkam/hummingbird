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
    // Paths to static filter-rule files, relative to the extension directory
    // (story 9.4.1). Modelled on MV3's
    // `declarative_net_request.rule_resources`.
    //
    // Static rulesets exist so rules survive a restart without the host needing
    // any persistence: they are declared, not registered, so they are simply
    // read again on the next run. They are also loaded before any background
    // script runs, which removes an ordering coupling — otherwise the first
    // page's subresources could race the script that was going to block them.
    std::vector<std::string> rule_resources;
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
