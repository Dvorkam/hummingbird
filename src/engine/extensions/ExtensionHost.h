#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "core/platform_api/IScriptEngine.h"
#include "engine/extensions/ExtensionLoader.h"
#include "engine/extensions/ExtensionSettings.h"

namespace Hummingbird::Engine {

struct ExtensionRuntimeError {
    std::string extension_name;
    std::string message;
    std::filesystem::path path;
};

class ExtensionHost {
public:
    using ScriptEngineFactory = std::function<Hummingbird::ScriptEnginePtr()>;

    explicit ExtensionHost(ScriptEngineFactory create_engine);

    void set_extensions(std::vector<LoadedExtension> extensions);
    void set_settings(ExtensionSettings settings);
    size_t extension_count() const;

    // Starts background scripts for all loaded extensions. This call is idempotent.
    void start_background_scripts();

    // Enables/disables an extension by ID. If disabling a started extension, it is torn down.
    // If enabling and background scripts have already been started, this will start it.
    bool set_extension_enabled(std::string_view id, bool enabled);

    // Tears down all extension runtimes.
    void shutdown();

    const std::vector<ExtensionRuntimeError>& errors() const { return errors_; }

private:
    struct Runtime {
        LoadedExtension extension;
        Hummingbird::ScriptEnginePtr engine;
        bool started = false;
    };

    bool start_background_script(Runtime& runtime);

    ScriptEngineFactory create_engine_;
    std::vector<Runtime> runtimes_;
    std::vector<ExtensionRuntimeError> errors_;
    ExtensionSettings settings_;
    bool started_ = false;
};

}  // namespace Hummingbird::Engine
