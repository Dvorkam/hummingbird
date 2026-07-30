#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "core/net/RequestFilter.h"
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

    // The profile's request filter, which static rulesets and the JS rules API
    // both write into (story 9.4.1). Null means an engine without filtering,
    // which is what most tests want. Set it before start_background_scripts().
    void set_request_filter(std::shared_ptr<Core::RequestFilter> filter);

    // Tab events (5.3.1/5.3.3). BrowserApp emits navigate events on committed document transitions.
    void notify_tab_created(TabId id, std::string_view url);
    void notify_tab_activated(TabId id);
    void notify_tab_navigated(TabId id, std::string_view url);

    bool insert_css(std::string_view extension_id, std::uint32_t tab_id, std::string_view css_text) override;

    // Whether `extension_id` is loaded, enabled, and declares `permission`
    // (story 9.4.1). Every gated API goes through this, so there is one place
    // that decides and one place to test.
    bool has_permission(std::string_view extension_id, std::string_view permission) const;

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
    const Runtime* find_runtime(std::string_view id) const;
    // Reads a runtime's manifest-declared rulesets into the filter. Runs BEFORE
    // its background script, so an extension's rules are in force for the first
    // request of the session rather than whenever its script happens to finish.
    void load_static_rules(const Runtime& runtime);

    ScriptEngineFactory create_engine_;
    std::vector<Runtime> runtimes_;
    std::vector<ExtensionRuntimeError> errors_;
    ExtensionSettings settings_;
    InsertCssHandler insert_css_handler_;
    std::shared_ptr<Core::RequestFilter> request_filter_;
    bool started_ = false;
};

}  // namespace Hummingbird::Engine
