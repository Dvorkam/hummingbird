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
    std::string error = "boom";
};

class RecordingScriptEngine final : public Hummingbird::IScriptEngine {
public:
    explicit RecordingScriptEngine(EvalSink* sink) : sink_(sink) {}

    void bind_host(Hummingbird::IScriptHost* /*host*/) override {}

    Hummingbird::ScriptEvalResult eval(std::string_view source, std::string_view filename) override {
        if (sink_) {
            sink_->evals.push_back(EvalRecord{std::string(filename), std::string(source)});
            if (sink_->fail) {
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
    ASSERT_EQ(sink.evals.size(), 2u);

    host.start_background_scripts();
    EXPECT_EQ(sink.evals.size(), 2u);
    EXPECT_TRUE(host.errors().empty());

    host.shutdown();
    host.start_background_scripts();
    EXPECT_EQ(sink.evals.size(), 4u);
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

    Hummingbird::Engine::ExtensionHost host([&sink]() { return std::make_unique<RecordingScriptEngine>(&sink); });
    host.set_extensions(std::move(extensions));
    host.start_background_scripts();

    ASSERT_EQ(sink.evals.size(), 1u);
    ASSERT_EQ(host.errors().size(), 1u);
    EXPECT_EQ(host.errors()[0].extension_name, "E");
    EXPECT_FALSE(host.errors()[0].message.empty());
}
