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

size_t ExtensionHost::extension_count() const {
    return runtimes_.size();
}

void ExtensionHost::start_background_scripts() {
    errors_.clear();

    for (auto& runtime : runtimes_) {
        if (runtime.started) {
            continue;
        }

        if (!runtime.engine) {
            runtime.engine = create_engine_ ? create_engine_() : nullptr;
        }
        if (!runtime.engine) {
            errors_.push_back(ExtensionRuntimeError{runtime.extension.manifest.name, "failed to create script engine",
                                                    runtime.extension.root_dir});
            HB_LOG_WARN("[ext] failed to create script engine for " << runtime.extension.manifest.name);
            continue;
        }

        auto source = read_file_to_string(runtime.extension.background_entry_path);
        if (!source) {
            errors_.push_back(ExtensionRuntimeError{runtime.extension.manifest.name, "failed to read background script",
                                                    runtime.extension.background_entry_path});
            HB_LOG_WARN("[ext] failed to read background script for "
                        << runtime.extension.manifest.name << ": " << runtime.extension.background_entry_path.string());
            continue;
        }

        HB_LOG_INFO("[ext] starting background script: " << runtime.extension.manifest.name);
        auto result = runtime.engine->eval(*source, runtime.extension.background_entry_path.string());
        if (!result.ok) {
            errors_.push_back(ExtensionRuntimeError{runtime.extension.manifest.name, result.error,
                                                    runtime.extension.background_entry_path});
            HB_LOG_WARN("[ext] background script error in " << runtime.extension.manifest.name << ": " << result.error);
            continue;
        }

        runtime.started = true;
    }
}

void ExtensionHost::shutdown() {
    for (auto& runtime : runtimes_) {
        runtime.engine.reset();
        runtime.started = false;
    }
    errors_.clear();
}

}  // namespace Hummingbird::Engine
