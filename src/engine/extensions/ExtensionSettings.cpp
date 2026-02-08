#include "engine/extensions/ExtensionSettings.h"

#include <cstdlib>
#include <string>
#include <string_view>
#include <utility>

namespace Hummingbird::Engine {

namespace {
void parse_csv_env(const char* name, std::unordered_set<std::string>& out) {
    const char* raw = std::getenv(name);
    if (!raw || !raw[0]) return;
    std::string_view view(raw);
    size_t i = 0;
    while (i < view.size()) {
        while (i < view.size() && (view[i] == ' ' || view[i] == '\t' || view[i] == ',')) ++i;
        size_t start = i;
        while (i < view.size() && view[i] != ',') ++i;
        size_t end = i;
        while (end > start && (view[end - 1] == ' ' || view[end - 1] == '\t')) --end;
        if (end > start) {
            out.insert(std::string(view.substr(start, end - start)));
        }
        if (i < view.size() && view[i] == ',') ++i;
    }
}
}  // namespace

ExtensionSettings extension_settings_from_env() {
    ExtensionSettings settings;
    parse_csv_env("HB_EXTENSIONS_ENABLE", settings.enabled_ids);
    parse_csv_env("HB_EXTENSIONS_DISABLE", settings.disabled_ids);
    return settings;
}

std::string extension_id(const LoadedExtension& ext) {
    auto name = ext.root_dir.filename().string();
    if (!name.empty()) return name;
    // Fallback to manifest.name for safety, but prefer stable folder IDs.
    return ext.manifest.name;
}

bool is_extension_enabled(const ExtensionSettings& settings, const LoadedExtension& ext) {
    const auto id = extension_id(ext);
    if (!settings.enabled_ids.empty()) {
        return settings.enabled_ids.count(id) > 0;
    }
    if (settings.disabled_ids.count(id) > 0) {
        return false;
    }
    return true;
}

}  // namespace Hummingbird::Engine
