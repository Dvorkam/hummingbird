#include "engine/extensions/ExtensionHost.h"

#include <fstream>
#include <iterator>
#include <optional>
#include <utility>

#include "core/utils/Log.h"

namespace Hummingbird::Engine {

namespace {
std::optional<std::string> read_file_to_string(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file) return std::nullopt;
    return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}
}  // namespace

ExtensionHost::ExtensionHost(ScriptEngineFactory create_engine) : create_engine_(std::move(create_engine)) {}

void ExtensionHost::set_extensions(std::vector<LoadedExtension> extensions) {
    shutdown();
    runtimes_.clear();
    runtimes_.reserve(extensions.size());
    for (auto& ext : extensions) {
        Runtime runtime;
        runtime.extension = std::move(ext);
        runtimes_.push_back(std::move(runtime));
    }
}

void ExtensionHost::set_settings(ExtensionSettings settings) {
    settings_ = std::move(settings);
}

size_t ExtensionHost::extension_count() const {
    return runtimes_.size();
}

bool ExtensionHost::start_background_script(Runtime& runtime) {
    if (runtime.started) {
        return true;
    }
    if (!is_extension_enabled(settings_, runtime.extension)) {
        return false;
    }

    if (!runtime.engine) {
        runtime.engine = create_engine_ ? create_engine_() : nullptr;
    }
    if (!runtime.engine) {
        errors_.push_back(ExtensionRuntimeError{runtime.extension.manifest.name, "failed to create script engine",
                                                runtime.extension.root_dir});
        HB_LOG_WARN("[ext] failed to create script engine for " << runtime.extension.manifest.name);
        return false;
    }

    auto source = read_file_to_string(runtime.extension.background_entry_path);
    if (!source) {
        errors_.push_back(ExtensionRuntimeError{runtime.extension.manifest.name, "failed to read background script",
                                                runtime.extension.background_entry_path});
        HB_LOG_WARN("[ext] failed to read background script for " << runtime.extension.manifest.name << ": "
                                                                  << runtime.extension.background_entry_path.string());
        return false;
    }

    HB_LOG_INFO("[ext] starting background script: " << runtime.extension.manifest.name);
    auto result = runtime.engine->eval(*source, runtime.extension.background_entry_path.string());
    if (!result.ok) {
        errors_.push_back(ExtensionRuntimeError{runtime.extension.manifest.name, result.error,
                                                runtime.extension.background_entry_path});
        HB_LOG_WARN("[ext] background script error in " << runtime.extension.manifest.name << ": " << result.error);
        return false;
    }

    runtime.started = true;
    return true;
}

void ExtensionHost::start_background_scripts() {
    errors_.clear();
    started_ = true;

    for (auto& runtime : runtimes_) {
        (void)start_background_script(runtime);
    }
}

bool ExtensionHost::set_extension_enabled(std::string_view id, bool enabled) {
    const std::string id_str(id);
    const bool allowlist_mode = !settings_.enabled_ids.empty();

    if (enabled) {
        settings_.disabled_ids.erase(id_str);
        if (allowlist_mode) {
            settings_.enabled_ids.insert(id_str);
        }
    } else {
        if (allowlist_mode) {
            settings_.enabled_ids.erase(id_str);
        } else {
            settings_.disabled_ids.insert(id_str);
        }
    }

    bool found = false;
    for (auto& runtime : runtimes_) {
        if (extension_id(runtime.extension) != id_str) {
            continue;
        }
        found = true;
        if (!enabled) {
            runtime.engine.reset();
            runtime.started = false;
        } else if (started_) {
            (void)start_background_script(runtime);
        }
    }
    return found;
}

void ExtensionHost::shutdown() {
    for (auto& runtime : runtimes_) {
        runtime.engine.reset();
        runtime.started = false;
    }
    errors_.clear();
    started_ = false;
}

}  // namespace Hummingbird::Engine
