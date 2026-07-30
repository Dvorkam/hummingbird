#include "engine/extensions/ExtensionHost.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/platform_api/ResourceProviderFactory.h"
#include "core/platform_api/ScriptEngineFactory.h"
#include "test_utils/HeadlessTabHarness.h"
#include "test_utils/TestFakes.h"

namespace {
class TempDirGuard {
public:
    explicit TempDirGuard(std::filesystem::path path) : path_(std::move(path)) {
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
        std::filesystem::create_directories(path_, ec);
    }

    ~TempDirGuard() {
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
    }

    const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
};

void write_text(const std::filesystem::path& path, const std::string& text) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path.string(), std::ios::out | std::ios::binary);
    file << text;
}

struct EvalRecord {
    std::string filename;
    std::string source;
};

struct EvalSink {
    std::vector<EvalRecord> evals;
    bool fail = false;
    size_t fail_after = 0;
    std::string error = "boom";
};

class RecordingScriptEngine final : public Hummingbird::IScriptEngine {
public:
    explicit RecordingScriptEngine(EvalSink* sink) : sink_(sink) {}

    void bind_host(Hummingbird::IScriptHost* /*host*/) override {}
    // Captured so a test can call back into the host exactly as the extension's
    // own script would, with the identity the host itself assigned (9.4.1).
    void bind_extension_host(Hummingbird::IExtensionApiHost* host, std::string_view extension_id) override {
        bound_host = host;
        bound_extension_id = std::string(extension_id);
    }

    Hummingbird::IExtensionApiHost* bound_host = nullptr;
    std::string bound_extension_id;

    Hummingbird::ScriptEvalResult eval(std::string_view source, std::string_view filename) override {
        const size_t call_index = sink_ ? sink_->evals.size() : 0u;
        if (sink_) {
            sink_->evals.push_back(EvalRecord{std::string(filename), std::string(source)});
            if (sink_->fail && call_index >= sink_->fail_after) {
                return Hummingbird::ScriptEvalResult{false, sink_->error};
            }
        }
        return Hummingbird::ScriptEvalResult{true, {}};
    }

private:
    EvalSink* sink_ = nullptr;
};

// Permissions default to what the real bundled `dark-mode` manifest declares,
// so a fixture behaves like a correctly-declared extension. Tests that care
// about the 9.4.1 permission gate pass their own list — including an empty one.
Hummingbird::Engine::LoadedExtension make_loaded_extension(const std::filesystem::path& root, std::string name,
                                                           const std::string& script_name,
                                                           std::vector<std::string> permissions = {"tabs",
                                                                                                   "scripting"}) {
    Hummingbird::Engine::LoadedExtension ext;
    ext.root_dir = root;
    ext.manifest_path = root / "manifest.json";
    ext.manifest.name = std::move(name);
    ext.manifest.version = "0.0.1";
    ext.manifest.background_entry = script_name;
    ext.manifest.permissions = std::move(permissions);
    ext.background_entry_path = root / script_name;
    return ext;
}
}  // namespace

TEST(ExtensionHostTest, StartsBackgroundScriptsOncePerExtension) {
    TempDirGuard root(std::filesystem::temp_directory_path() / "hummingbird-ext-host-test");
    auto ext_a_root = root.path() / "a";
    auto ext_b_root = root.path() / "b";

    write_text(ext_a_root / "bg.js", "globalThis.__a = 1;");
    write_text(ext_b_root / "bg.js", "globalThis.__b = 2;");

    std::vector<Hummingbird::Engine::LoadedExtension> extensions;
    extensions.push_back(make_loaded_extension(ext_a_root, "A", "bg.js"));
    extensions.push_back(make_loaded_extension(ext_b_root, "B", "bg.js"));

    EvalSink sink;
    Hummingbird::Engine::ExtensionHost host([&sink]() { return std::make_unique<RecordingScriptEngine>(&sink); });
    host.set_extensions(std::move(extensions));

    host.start_background_scripts();
    ASSERT_EQ(sink.evals.size(), 4u);  // bootstrap + background per extension

    host.start_background_scripts();
    EXPECT_EQ(sink.evals.size(), 4u);
    EXPECT_TRUE(host.errors().empty());

    host.shutdown();
    host.start_background_scripts();
    EXPECT_EQ(sink.evals.size(), 8u);
}

TEST(ExtensionHostTest, SurfacesEvalErrorsPerExtension) {
    TempDirGuard root(std::filesystem::temp_directory_path() / "hummingbird-ext-host-test-errors");
    auto ext_root = root.path() / "e";
    write_text(ext_root / "bg.js", "throw new Error('fail');");

    std::vector<Hummingbird::Engine::LoadedExtension> extensions;
    extensions.push_back(make_loaded_extension(ext_root, "E", "bg.js"));

    EvalSink sink;
    sink.fail = true;
    sink.error = "eval failed";
    sink.fail_after = 1;  // allow bootstrap, fail the background script

    Hummingbird::Engine::ExtensionHost host([&sink]() { return std::make_unique<RecordingScriptEngine>(&sink); });
    host.set_extensions(std::move(extensions));
    host.start_background_scripts();

    ASSERT_EQ(sink.evals.size(), 2u);  // bootstrap + background
    ASSERT_EQ(host.errors().size(), 1u);
    EXPECT_EQ(host.errors()[0].extension_name, "E");
    EXPECT_FALSE(host.errors()[0].message.empty());
}

TEST(ExtensionHostTest, DisabledExtensionsDoNotStartAndCanBeToggled) {
    TempDirGuard root(std::filesystem::temp_directory_path() / "hummingbird-ext-host-test-disabled");
    auto ext_a_root = root.path() / "a";
    auto ext_b_root = root.path() / "b";

    write_text(ext_a_root / "bg.js", "globalThis.__a = 1;");
    write_text(ext_b_root / "bg.js", "globalThis.__b = 2;");

    std::vector<Hummingbird::Engine::LoadedExtension> extensions;
    extensions.push_back(make_loaded_extension(ext_a_root, "A", "bg.js"));
    extensions.push_back(make_loaded_extension(ext_b_root, "B", "bg.js"));

    EvalSink sink;
    Hummingbird::Engine::ExtensionHost host([&sink]() { return std::make_unique<RecordingScriptEngine>(&sink); });

    Hummingbird::Engine::ExtensionSettings settings;
    settings.disabled_ids.insert("b");
    host.set_settings(std::move(settings));
    host.set_extensions(std::move(extensions));

    host.start_background_scripts();
    ASSERT_EQ(sink.evals.size(), 2u);  // bootstrap + background for A

    EXPECT_FALSE(host.set_extension_enabled("nope", false));
    EXPECT_TRUE(host.set_extension_enabled("b", true));

    ASSERT_EQ(sink.evals.size(), 4u);  // bootstrap + background for B
    EXPECT_TRUE(host.set_extension_enabled("b", false));

    // Disabling should tear down; re-enabling should start again.
    EXPECT_TRUE(host.set_extension_enabled("b", true));
    EXPECT_EQ(sink.evals.size(), 6u);
}

TEST(ExtensionHostTest, TabEventDispatchEvaluatesEmitCalls) {
    TempDirGuard root(std::filesystem::temp_directory_path() / "hummingbird-ext-host-test-events");
    auto ext_root = root.path() / "dark-mode";
    write_text(ext_root / "bg.js",
               "browser.tabs.onCreated.addListener(function(t){ console.log('created', t.id); });"
               "browser.tabs.onActivated.addListener(function(t){ console.log('activated', t.id); });"
               "browser.tabs.onNavigated.addListener(function(t){ console.log('navigated', t.id); });");

    std::vector<Hummingbird::Engine::LoadedExtension> extensions;
    extensions.push_back(make_loaded_extension(ext_root, "Dark", "bg.js"));

    EvalSink sink;
    Hummingbird::Engine::ExtensionHost host([&sink]() { return std::make_unique<RecordingScriptEngine>(&sink); });
    host.set_extensions(std::move(extensions));
    host.start_background_scripts();

    host.notify_tab_created(7, "https://example.dev");
    host.notify_tab_activated(7);
    host.notify_tab_navigated(7, "https://example.dev/js");

    bool saw_created = false;
    bool saw_activated = false;
    bool saw_navigated = false;
    for (const auto& eval : sink.evals) {
        if (eval.source.find("__hb_emitTabCreated") != std::string::npos) saw_created = true;
        if (eval.source.find("__hb_emitTabActivated") != std::string::npos) saw_activated = true;
        if (eval.source.find("__hb_emitTabNavigated") != std::string::npos) saw_navigated = true;
    }
    EXPECT_TRUE(saw_created);
    EXPECT_TRUE(saw_activated);
    EXPECT_TRUE(saw_navigated);
}

TEST(ExtensionHostTest, InsertCssApiRoutesToHostHandler) {
    TempDirGuard root(std::filesystem::temp_directory_path() / "hummingbird-ext-host-test-insert-css");
    auto ext_root = root.path() / "dark-mode";
    write_text(ext_root / "bg.js",
               "const ok = browser.scripting.insertCSS({tabId: 17, cssText: 'body { color: red; }'});"
               "console.log('insert css', ok);");

    std::vector<Hummingbird::Engine::LoadedExtension> extensions;
    extensions.push_back(make_loaded_extension(ext_root, "Dark", "bg.js"));

    int called = 0;
    Hummingbird::Engine::TabId last_id = 0;
    std::string last_css;
    Hummingbird::Engine::ExtensionHost host([]() { return Hummingbird::create_script_engine(); });
    host.set_insert_css_handler([&](Hummingbird::Engine::TabId tab_id, std::string_view css_text) {
        ++called;
        last_id = tab_id;
        last_css = std::string(css_text);
        return true;
    });
    host.set_extensions(std::move(extensions));
    host.start_background_scripts();

    EXPECT_EQ(called, 1);
    EXPECT_EQ(last_id, 17u);
    EXPECT_EQ(last_css, "body { color: red; }");
    EXPECT_TRUE(host.errors().empty());
}

// --- 9.4.1 permission enforcement -------------------------------------------
// `permissions` was parsed and never enforced until this story, because the host
// had no way to tell who was calling. These pin the enforcement, and the DENIAL
// case is the one that matters: a gate only tested on the allow path is
// indistinguishable from no gate.

TEST(ExtensionHostTest, AnExtensionWithoutTheScriptingPermissionCannotInjectCss) {
    TempDirGuard root(std::filesystem::temp_directory_path() / "hummingbird-ext-host-test-css-denied");
    auto ext_root = root.path() / "nosy";
    write_text(ext_root / "bg.js",
               "globalThis.__result = browser.scripting.insertCSS({tabId: 5, cssText: 'body{}'});");

    std::vector<Hummingbird::Engine::LoadedExtension> extensions;
    // Declares "tabs" but NOT "scripting".
    extensions.push_back(make_loaded_extension(ext_root, "Nosy", "bg.js", {"tabs"}));

    int called = 0;
    Hummingbird::Engine::ExtensionHost host([]() { return Hummingbird::create_script_engine(); });
    host.set_insert_css_handler([&](Hummingbird::Engine::TabId, std::string_view) {
        ++called;
        return true;
    });
    host.set_extensions(std::move(extensions));
    host.start_background_scripts();

    EXPECT_EQ(called, 0) << "the handler must never run for an undeclared permission";
    // Refused, not crashed: an extension asking for something it may not have is
    // an ordinary answer, not an error the user should see.
    EXPECT_TRUE(host.errors().empty());
}

TEST(ExtensionHostTest, PermissionsAreCheckedPerExtensionNotGlobally) {
    TempDirGuard root(std::filesystem::temp_directory_path() / "hummingbird-ext-host-test-css-mixed");
    auto allowed_root = root.path() / "allowed";
    auto denied_root = root.path() / "denied";
    write_text(allowed_root / "bg.js", "browser.scripting.insertCSS({tabId: 1, cssText: 'a{}'});");
    write_text(denied_root / "bg.js", "browser.scripting.insertCSS({tabId: 2, cssText: 'b{}'});");

    std::vector<Hummingbird::Engine::LoadedExtension> extensions;
    extensions.push_back(make_loaded_extension(allowed_root, "Allowed", "bg.js", {"scripting"}));
    extensions.push_back(make_loaded_extension(denied_root, "Denied", "bg.js", {}));

    std::vector<Hummingbird::Engine::TabId> injected;
    Hummingbird::Engine::ExtensionHost host([]() { return Hummingbird::create_script_engine(); });
    host.set_insert_css_handler([&](Hummingbird::Engine::TabId tab_id, std::string_view) {
        injected.push_back(tab_id);
        return true;
    });
    host.set_extensions(std::move(extensions));
    host.start_background_scripts();

    // One host serves both contexts, so this is the test that the identity
    // actually travels: if it did not, both would be allowed or both refused.
    ASSERT_EQ(injected.size(), 1u);
    EXPECT_EQ(injected.front(), 1u) << "only the extension that declared the permission got through";
}

TEST(ExtensionHostTest, TheHostRefusesApiCallsFromAnUnknownExtensionId) {
    TempDirGuard root(std::filesystem::temp_directory_path() / "hummingbird-ext-host-test-unknown-id");
    auto ext_root = root.path() / "real";
    write_text(ext_root / "bg.js", "");

    std::vector<Hummingbird::Engine::LoadedExtension> extensions;
    extensions.push_back(make_loaded_extension(ext_root, "Real", "bg.js"));

    Hummingbird::Engine::ExtensionHost host([]() { return Hummingbird::create_script_engine(); });
    host.set_insert_css_handler([](Hummingbird::Engine::TabId, std::string_view) { return true; });
    host.set_extensions(std::move(extensions));
    host.start_background_scripts();

    EXPECT_TRUE(host.has_permission("real", "scripting"));
    // "I do not know who this is" must not resolve to "let them".
    EXPECT_FALSE(host.has_permission("ghost", "scripting"));
    EXPECT_FALSE(host.insert_css("ghost", 1, "body{}"));
}

// M5 lifecycle: a disabled extension must not still be acting.
TEST(ExtensionHostTest, ADisabledExtensionHasNoPermissions) {
    TempDirGuard root(std::filesystem::temp_directory_path() / "hummingbird-ext-host-test-disabled-perm");
    auto ext_root = root.path() / "dark-mode";
    write_text(ext_root / "bg.js", "");

    std::vector<Hummingbird::Engine::LoadedExtension> extensions;
    extensions.push_back(make_loaded_extension(ext_root, "Dark", "bg.js"));

    Hummingbird::Engine::ExtensionSettings settings;
    settings.disabled_ids.insert("dark-mode");

    Hummingbird::Engine::ExtensionHost host([]() { return Hummingbird::create_script_engine(); });
    // A handler that always succeeds, so the ONLY thing that can make the call
    // below return false is the permission gate. Without this the assertion
    // passes on the "no handler installed" path and proves nothing.
    host.set_insert_css_handler([](Hummingbird::Engine::TabId, std::string_view) { return true; });
    host.set_settings(settings);
    host.set_extensions(std::move(extensions));
    host.start_background_scripts();

    EXPECT_FALSE(host.has_permission("dark-mode", "scripting"));
    EXPECT_FALSE(host.insert_css("dark-mode", 1, "body{}"));
}

TEST(ExtensionHostTest, NavigatedListenerCanInjectCssForEventTab) {
    TempDirGuard root(std::filesystem::temp_directory_path() / "hummingbird-ext-host-test-nav-insert-css");
    auto ext_root = root.path() / "dark-mode";
    write_text(ext_root / "bg.js",
               "browser.tabs.onNavigated.addListener(function(tab){"
               "  browser.scripting.insertCSS({tabId: tab.id, cssText: 'p { color: #222; }'});"
               "});");

    std::vector<Hummingbird::Engine::LoadedExtension> extensions;
    extensions.push_back(make_loaded_extension(ext_root, "Dark", "bg.js"));

    int called = 0;
    Hummingbird::Engine::TabId last_id = 0;
    std::string last_css;
    Hummingbird::Engine::ExtensionHost host([]() { return Hummingbird::create_script_engine(); });
    host.set_insert_css_handler([&](Hummingbird::Engine::TabId tab_id, std::string_view css_text) {
        ++called;
        last_id = tab_id;
        last_css = std::string(css_text);
        return true;
    });
    host.set_extensions(std::move(extensions));
    host.start_background_scripts();
    host.notify_tab_navigated(23, "https://example.dev");

    EXPECT_EQ(called, 1);
    EXPECT_EQ(last_id, 23u);
    EXPECT_EQ(last_css, "p { color: #222; }");
    EXPECT_TRUE(host.errors().empty());
}

TEST(ExtensionHostTest, HeadlessNavigatedEventAppliesInjectedCssToTabPipeline) {
    TempDirGuard root(std::filesystem::temp_directory_path() / "hummingbird-ext-host-test-headless-loop");
    auto ext_root = root.path() / "dark-mode";
    write_text(ext_root / "bg.js",
               "browser.tabs.onNavigated.addListener(function(tab){"
               "  browser.scripting.insertCSS({tabId: tab.id, cssText: \"body { background-image: "
               "url('/ext-event.png'); }\"});"
               "});");

    std::vector<Hummingbird::Engine::LoadedExtension> extensions;
    extensions.push_back(make_loaded_extension(ext_root, "Dark", "bg.js"));

    auto provider = Hummingbird::create_resource_provider();
    ASSERT_NE(provider, nullptr);

    auto network = std::make_unique<Hummingbird::Test::RoutingNetwork>();
    network->set_response("https://acme.test", "<!doctype html><html><body><p>headless</p></body></html>");
    network->set_response("https://acme.test/ext-event.png", "PNGDATA");
    auto fallback = std::make_unique<Hummingbird::Test::RoutingNetwork>();
    Hummingbird::Test::HeadlessTabHarness harness(std::move(network), std::move(fallback), std::move(provider),
                                                  std::make_unique<Hummingbird::Test::InlineImageDecoder>());

    Hummingbird::Engine::ExtensionHost host([]() { return Hummingbird::create_script_engine(); });
    host.set_insert_css_handler([&](Hummingbird::Engine::TabId tab_id, std::string_view css_text) {
        if (tab_id != 1u) return false;
        return harness.tab().insert_extension_css(css_text);
    });
    host.set_extensions(std::move(extensions));
    host.start_background_scripts();

    harness.navigate("https://acme.test");
    ASSERT_TRUE(harness.tick());

    auto before = harness.resource_view("https://acme.test/ext-event.png", Hummingbird::Engine::ResourceType::Image);
    EXPECT_FALSE(before.has_value());

    host.notify_tab_navigated(1, "https://acme.test");
    EXPECT_TRUE(harness.tick());
    EXPECT_TRUE(harness.tick());

    auto after = harness.resource_view("https://acme.test/ext-event.png", Hummingbird::Engine::ResourceType::Image);
    ASSERT_TRUE(after.has_value());
    EXPECT_EQ(after->state, Hummingbird::Engine::ResourceState::Ready);
    EXPECT_TRUE(host.errors().empty());
}

TEST(ExtensionHostTest, DisabledExtensionDoesNotReceiveEventsUntilReenabled) {
    TempDirGuard root(std::filesystem::temp_directory_path() / "hummingbird-ext-host-test-disable-events");
    auto ext_root = root.path() / "dark-mode";
    write_text(ext_root / "bg.js",
               "browser.tabs.onNavigated.addListener(function(tab){"
               "  browser.scripting.insertCSS({tabId: tab.id, cssText: 'body { color: #444; }'});"
               "});");

    std::vector<Hummingbird::Engine::LoadedExtension> extensions;
    extensions.push_back(make_loaded_extension(ext_root, "Dark", "bg.js"));

    int called = 0;
    Hummingbird::Engine::ExtensionHost host([]() { return Hummingbird::create_script_engine(); });
    host.set_insert_css_handler([&](Hummingbird::Engine::TabId, std::string_view) {
        ++called;
        return true;
    });
    host.set_extensions(std::move(extensions));
    host.start_background_scripts();

    host.notify_tab_navigated(1, "https://example.dev");
    EXPECT_EQ(called, 1);

    EXPECT_TRUE(host.set_extension_enabled("dark-mode", false));
    host.notify_tab_navigated(1, "https://example.dev/disabled");
    EXPECT_EQ(called, 1);

    EXPECT_TRUE(host.set_extension_enabled("dark-mode", true));
    host.notify_tab_navigated(1, "https://example.dev/reenabled");
    EXPECT_EQ(called, 2);
    EXPECT_TRUE(host.errors().empty());
}

TEST(ExtensionHostTest, ShutdownIsIdempotentAndStopsEventCallbacks) {
    TempDirGuard root(std::filesystem::temp_directory_path() / "hummingbird-ext-host-test-shutdown-events");
    auto ext_root = root.path() / "dark-mode";
    write_text(ext_root / "bg.js",
               "browser.tabs.onNavigated.addListener(function(tab){"
               "  browser.scripting.insertCSS({tabId: tab.id, cssText: 'body { color: #555; }'});"
               "});");

    std::vector<Hummingbird::Engine::LoadedExtension> extensions;
    extensions.push_back(make_loaded_extension(ext_root, "Dark", "bg.js"));

    int called = 0;
    Hummingbird::Engine::ExtensionHost host([]() { return Hummingbird::create_script_engine(); });
    host.set_insert_css_handler([&](Hummingbird::Engine::TabId, std::string_view) {
        ++called;
        return true;
    });
    host.set_extensions(std::move(extensions));
    host.start_background_scripts();

    host.notify_tab_navigated(1, "https://example.dev");
    EXPECT_EQ(called, 1);

    host.shutdown();
    host.shutdown();
    host.notify_tab_navigated(1, "https://example.dev/after-shutdown");
    EXPECT_EQ(called, 1);
}
