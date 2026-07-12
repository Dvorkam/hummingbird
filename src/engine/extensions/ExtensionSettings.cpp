#include "engine/extensions/ExtensionSettings.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>

#include "core/utils/AssetPath.h"
#include "core/utils/StringUtils.h"

namespace Hummingbird::Engine {

namespace {
using Hummingbird::Core::Utils::trim_ascii_whitespace;

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

bool parse_enabled_value(std::string_view value, bool* enabled) {
    if (!enabled) {
        return false;
    }
    auto lowered = Hummingbird::Core::Utils::to_lower(trim_ascii_whitespace(value));
    if (lowered == "1" || lowered == "true" || lowered == "yes" || lowered == "on" || lowered == "enabled") {
        *enabled = true;
        return true;
    }
    if (lowered == "0" || lowered == "false" || lowered == "no" || lowered == "off" || lowered == "disabled") {
        *enabled = false;
        return true;
    }
    return false;
}
}  // namespace

ExtensionSettings extension_settings_from_env() {
    ExtensionSettings settings;
    parse_csv_env("HB_EXTENSIONS_ENABLE", settings.enabled_ids);
    parse_csv_env("HB_EXTENSIONS_DISABLE", settings.disabled_ids);
    return settings;
}

ExtensionSettings extension_settings_from_ini(std::string_view ini_text) {
    ExtensionSettings settings;
    enum class Section { Unknown, Extensions };
    Section section = Section::Unknown;

    size_t cursor = 0;
    while (cursor < ini_text.size()) {
        size_t line_end = ini_text.find('\n', cursor);
        if (line_end == std::string_view::npos) {
            line_end = ini_text.size();
        }
        std::string_view line = ini_text.substr(cursor, line_end - cursor);
        cursor = line_end == ini_text.size() ? line_end : line_end + 1;
        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1);
        }
        line = trim_ascii_whitespace(line);
        if (line.empty() || line.front() == ';' || line.front() == '#') {
            continue;
        }

        if (line.front() == '[' && line.back() == ']') {
            auto section_name = trim_ascii_whitespace(line.substr(1, line.size() - 2));
            section = Hummingbird::Core::Utils::equals_ignore_case(section_name, "extensions") ? Section::Extensions
                                                                                               : Section::Unknown;
            continue;
        }
        if (section != Section::Extensions) {
            continue;
        }

        size_t equals = line.find('=');
        if (equals == std::string_view::npos) {
            continue;
        }
        auto key = trim_ascii_whitespace(line.substr(0, equals));
        auto value = trim_ascii_whitespace(line.substr(equals + 1));
        if (key.empty() || value.empty()) {
            continue;
        }

        bool enabled = true;
        if (!parse_enabled_value(value, &enabled)) {
            continue;
        }
        std::string id(key);
        settings.enabled_ids.erase(id);
        settings.disabled_ids.erase(id);
        if (!enabled) {
            settings.disabled_ids.insert(std::move(id));
        }
    }

    return settings;
}

ExtensionSettings extension_settings_from_ini_file(const std::filesystem::path& ini_path) {
    std::ifstream file(ini_path, std::ios::in | std::ios::binary);
    if (!file) {
        return {};
    }
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return extension_settings_from_ini(content);
}

std::filesystem::path default_extension_settings_ini_path() {
    if (const char* configured = std::getenv("HB_SETTINGS_INI"); configured && configured[0]) {
        return std::filesystem::path(configured);
    }
    return Hummingbird::Core::Utils::resolve_asset_path("assets/config/browser.ini");
}

ExtensionSettings merge_extension_settings(ExtensionSettings base, const ExtensionSettings& overrides) {
    if (!overrides.enabled_ids.empty()) {
        base.enabled_ids = overrides.enabled_ids;
    }
    for (const auto& id : overrides.disabled_ids) {
        base.disabled_ids.insert(id);
    }
    for (const auto& id : overrides.enabled_ids) {
        base.disabled_ids.erase(id);
    }
    return base;
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
