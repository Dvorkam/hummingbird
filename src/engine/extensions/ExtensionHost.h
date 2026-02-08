#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "core/platform_api/IScriptEngine.h"
#include "engine/extensions/ExtensionLoader.h"

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
    size_t extension_count() const;

    // Starts background scripts for all loaded extensions. This call is idempotent.
    void start_background_scripts();

    // Tears down all extension runtimes.
    void shutdown();

    const std::vector<ExtensionRuntimeError>& errors() const { return errors_; }

private:
    struct Runtime {
        LoadedExtension extension;
        Hummingbird::ScriptEnginePtr engine;
        bool started = false;
    };

    ScriptEngineFactory create_engine_;
    std::vector<Runtime> runtimes_;
    std::vector<ExtensionRuntimeError> errors_;
};

}  // namespace Hummingbird::Engine
