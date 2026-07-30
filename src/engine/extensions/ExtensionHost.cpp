#include "engine/extensions/ExtensionHost.h"

#include <fstream>
#include <iterator>
#include <optional>
#include <sstream>
#include <utility>

#include "core/utils/Log.h"

namespace Hummingbird::Engine {

namespace {
std::optional<std::string> read_file_to_string(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file) return std::nullopt;
    return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

std::string js_string_literal(std::string_view input) {
    std::string out;
    out.reserve(input.size() + 2);
    out.push_back('"');
    for (char ch : input) {
        switch (ch) {
            case '\\':
                out.append("\\\\");
                break;
            case '"':
                out.append("\\\"");
                break;
            case '\n':
                out.append("\\n");
                break;
            case '\r':
                out.append("\\r");
                break;
            case '\t':
                out.append("\\t");
                break;
            default:
                out.push_back(ch);
                break;
        }
    }
    out.push_back('"');
    return out;
}

// Minimal WebExtension-like facade for 5.3.1.
constexpr std::string_view kExtensionBootstrap = R"JS(
(function(){
  if (!globalThis.__hb) globalThis.__hb = {};
  const hb = globalThis.__hb;
  if (!hb.tabs) hb.tabs = {};
  if (!hb.tabs.listeners) hb.tabs.listeners = { created: [], activated: [], navigated: [] };
  if (hb.tabs.activeTabId === undefined) hb.tabs.activeTabId = null;

  function safeCall(fn, arg) {
    try { fn(arg); } catch (e) { try { console.log("[ext] listener error", e && e.message ? e.message : e); } catch (_) {} }
  }

  globalThis.__hb_emitTabCreated = function(tab) {
    const list = hb.tabs.listeners.created;
    for (let i = 0; i < list.length; i++) safeCall(list[i], tab);
  };
  globalThis.__hb_emitTabActivated = function(tab) {
    const list = hb.tabs.listeners.activated;
    for (let i = 0; i < list.length; i++) safeCall(list[i], tab);
  };
  globalThis.__hb_emitTabNavigated = function(tab) {
    const list = hb.tabs.listeners.navigated;
    for (let i = 0; i < list.length; i++) safeCall(list[i], tab);
  };

  if (!globalThis.browser) globalThis.browser = {};
  if (!browser.tabs) browser.tabs = {};
  if (!browser.scripting) browser.scripting = {};

  browser.tabs.onCreated = { addListener: function(fn){ hb.tabs.listeners.created.push(fn); } };
  browser.tabs.onActivated = { addListener: function(fn){ hb.tabs.listeners.activated.push(fn); } };
  browser.tabs.onNavigated = { addListener: function(fn){ hb.tabs.listeners.navigated.push(fn); } };

  browser.tabs.active = function() {
    if (hb.tabs.activeTabId === null || hb.tabs.activeTabId === undefined) return null;
    return { id: hb.tabs.activeTabId };
  };

  browser.scripting.insertCSS = function(args) {
    if (!args || typeof args.cssText !== "string") return false;
    const id = Number(args.tabId);
    if (!Number.isInteger(id) || id < 0) return false;
    if (typeof globalThis.__hb_nativeInsertCss !== "function") return false;
    return !!globalThis.__hb_nativeInsertCss(id, args.cssText);
  };
})();
)JS";
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

    runtime.engine->bind_extension_host(this, extension_id(runtime.extension));

    auto bootstrap = runtime.engine->eval(kExtensionBootstrap, "hb-extension-bootstrap");
    if (!bootstrap.ok) {
        errors_.push_back(ExtensionRuntimeError{runtime.extension.manifest.name, bootstrap.error,
                                                runtime.extension.background_entry_path});
        HB_LOG_WARN("[ext] bootstrap error in " << runtime.extension.manifest.name << ": " << bootstrap.error);
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

void ExtensionHost::set_insert_css_handler(InsertCssHandler handler) {
    insert_css_handler_ = std::move(handler);
}

void ExtensionHost::eval_all_started(std::string_view source, std::string_view filename) {
    for (auto& runtime : runtimes_) {
        if (!runtime.started || !runtime.engine) continue;
        if (!is_extension_enabled(settings_, runtime.extension)) continue;
        (void)runtime.engine->eval(source, filename);
    }
}

void ExtensionHost::notify_tab_created(TabId id, std::string_view url) {
    std::ostringstream ss;
    ss << "globalThis.__hb_emitTabCreated({id:" << id << ",url:" << js_string_literal(url) << "});";
    eval_all_started(ss.str(), "hb-tab-created");
}

void ExtensionHost::notify_tab_activated(TabId id) {
    std::ostringstream ss;
    ss << "if (globalThis.__hb && globalThis.__hb.tabs) globalThis.__hb.tabs.activeTabId=" << id << ";";
    ss << "globalThis.__hb_emitTabActivated({id:" << id << "});";
    eval_all_started(ss.str(), "hb-tab-activated");
}

void ExtensionHost::notify_tab_navigated(TabId id, std::string_view url) {
    std::ostringstream ss;
    ss << "globalThis.__hb_emitTabNavigated({id:" << id << ",url:" << js_string_literal(url) << "});";
    eval_all_started(ss.str(), "hb-tab-navigated");
}

const ExtensionHost::Runtime* ExtensionHost::find_runtime(std::string_view id) const {
    for (const auto& runtime : runtimes_) {
        if (extension_id(runtime.extension) == id) return &runtime;
    }
    return nullptr;
}

bool ExtensionHost::has_permission(std::string_view extension_id_value, std::string_view permission) const {
    const Runtime* runtime = find_runtime(extension_id_value);
    if (!runtime) {
        // An unknown caller. Refuse rather than default-allow: an id that names
        // no loaded extension means the identity plumbing has broken, and the
        // safe reading of "I don't know who this is" is not "let them".
        HB_LOG_WARN("[ext] api call from unknown extension id: " << extension_id_value);
        return false;
    }
    // A disabled extension keeps its runtime around but must not act (M5).
    if (!is_extension_enabled(settings_, runtime->extension)) {
        return false;
    }
    if (!manifest_has_permission(runtime->extension.manifest, permission)) {
        HB_LOG_WARN("[ext] " << extension_id_value << " called an API needing the \"" << permission
                             << "\" permission, which its manifest does not declare");
        return false;
    }
    return true;
}

bool ExtensionHost::insert_css(std::string_view extension_id_value, std::uint32_t tab_id, std::string_view css_text) {
    // Gated as of 9.4.1. This API shipped ungated in M5 — not by choice, but
    // because the host could not tell who was calling. Now that it can, the
    // check belongs here as much as on the new rules API: shipping a
    // permission-gated API alongside an ungated one would leave the manifest
    // field meaning something in one place and nothing in the other.
    if (!has_permission(extension_id_value, "scripting")) {
        return false;
    }
    if (!insert_css_handler_) {
        return false;
    }
    return insert_css_handler_(tab_id, css_text);
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
