#include "engine/extensions/ExtensionHost.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

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

Hummingbird::Engine::LoadedExtension make_loaded_extension(const std::filesystem::path& root, std::string name,
                                                           const std::string& script_name) {
    Hummingbird::Engine::LoadedExtension ext;
    ext.root_dir = root;
    ext.manifest_path = root / "manifest.json";
    ext.manifest.name = std::move(name);
    ext.manifest.version = "0.0.1";
    ext.manifest.background_entry = script_name;
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
