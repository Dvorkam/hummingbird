#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "core/platform_api/IExtensionApiHost.h"
#include "core/platform_api/IScriptEngine.h"
#include "engine/extensions/ExtensionLoader.h"
#include "engine/extensions/ExtensionSettings.h"
#include "engine/tab/TabManager.h"

namespace Hummingbird::Engine {

struct ExtensionRuntimeError {
    std::string extension_name;
    std::string message;
    std::filesystem::path path;
};

class ExtensionHost : public Hummingbird::IExtensionApiHost {
public:
    using ScriptEngineFactory = std::function<Hummingbird::ScriptEnginePtr()>;
    using InsertCssHandler = std::function<bool(TabId, std::string_view)>;

    explicit ExtensionHost(ScriptEngineFactory create_engine);

    void set_extensions(std::vector<LoadedExtension> extensions);
    void set_settings(ExtensionSettings settings);
    size_t extension_count() const;

    // Starts background scripts for all loaded extensions. This call is idempotent.
    void start_background_scripts();

    // Enables/disables an extension by ID. If disabling a started extension, it is torn down.
    // If enabling and background scripts have already been started, this will start it.
    bool set_extension_enabled(std::string_view id, bool enabled);
    void set_insert_css_handler(InsertCssHandler handler);

    // Tab events (5.3.1/5.3.3). BrowserApp emits navigate events on committed document transitions.
    void notify_tab_created(TabId id, std::string_view url);
    void notify_tab_activated(TabId id);
    void notify_tab_navigated(TabId id, std::string_view url);

    bool insert_css(std::uint32_t tab_id, std::string_view css_text) override;

    // Tears down all extension runtimes.
    void shutdown();

    const std::vector<ExtensionRuntimeError>& errors() const { return errors_; }

private:
    struct Runtime {
        LoadedExtension extension;
        Hummingbird::ScriptEnginePtr engine;
        bool started = false;
    };

    void eval_all_started(std::string_view source, std::string_view filename);
    bool start_background_script(Runtime& runtime);

    ScriptEngineFactory create_engine_;
    std::vector<Runtime> runtimes_;
    std::vector<ExtensionRuntimeError> errors_;
    ExtensionSettings settings_;
    InsertCssHandler insert_css_handler_;
    bool started_ = false;
};

}  // namespace Hummingbird::Engine
