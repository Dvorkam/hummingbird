#include "engine/extensions/ExtensionLoader.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
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

std::string minimal_manifest(std::string name, std::string version, std::string entry) {
    return "{"
           "\"name\":\"" +
           name +
           "\","
           "\"version\":\"" +
           version +
           "\","
           "\"background\":{\"entry\":\"" +
           entry +
           "\"}"
           "}";
}
}  // namespace

TEST(ExtensionLoaderTest, LoadsExtensionsFromSubdirectoriesInStableOrder) {
    TempDirGuard root(std::filesystem::temp_directory_path() / "hummingbird-ext-loader-test");
    const auto ext_a = root.path() / "a_ext";
    const auto ext_b = root.path() / "b_ext";

    write_text(ext_b / "manifest.json", minimal_manifest("B", "1.0.0", "background.js"));
    write_text(ext_b / "background.js", "/* b */");

    write_text(ext_a / "manifest.json", minimal_manifest("A", "1.0.0", "bg.js"));
    write_text(ext_a / "bg.js", "/* a */");

    std::vector<Hummingbird::Engine::ExtensionLoadError> errors;
    auto loaded = Hummingbird::Engine::load_extensions_from_root(root.path(), &errors);

    ASSERT_EQ(loaded.size(), 2u);
    EXPECT_EQ(loaded[0].manifest.name, "A");
    EXPECT_EQ(loaded[0].root_dir.filename().string(), "a_ext");
    EXPECT_TRUE(std::filesystem::equivalent(loaded[0].background_entry_path, ext_a / "bg.js"));

    EXPECT_EQ(loaded[1].manifest.name, "B");
    EXPECT_EQ(loaded[1].root_dir.filename().string(), "b_ext");
    EXPECT_TRUE(std::filesystem::equivalent(loaded[1].background_entry_path, ext_b / "background.js"));

    EXPECT_TRUE(errors.empty());
}

TEST(ExtensionLoaderTest, ReportsInvalidManifestAndContinues) {
    TempDirGuard root(std::filesystem::temp_directory_path() / "hummingbird-ext-loader-test-invalid");
    const auto ok = root.path() / "ok";
    const auto bad = root.path() / "bad";

    write_text(ok / "manifest.json", minimal_manifest("OK", "1.0.0", "bg.js"));
    write_text(ok / "bg.js", "/* ok */");

    // Missing required fields -> parse should fail.
    write_text(bad / "manifest.json", "{\"name\":\"Bad\"}");
    write_text(bad / "bg.js", "/* bad */");

    std::vector<Hummingbird::Engine::ExtensionLoadError> errors;
    auto loaded = Hummingbird::Engine::load_extensions_from_root(root.path(), &errors);

    ASSERT_EQ(loaded.size(), 1u);
    EXPECT_EQ(loaded[0].manifest.name, "OK");
    ASSERT_FALSE(errors.empty());
}

TEST(ExtensionLoaderTest, RejectsEntryThatEscapesExtensionRoot) {
    TempDirGuard root(std::filesystem::temp_directory_path() / "hummingbird-ext-loader-test-escape");
    const auto ext = root.path() / "escape";

    write_text(ext / "manifest.json", minimal_manifest("Escape", "1.0.0", "../evil.js"));
    write_text(root.path() / "evil.js", "/* evil */");

    std::vector<Hummingbird::Engine::ExtensionLoadError> errors;
    auto loaded = Hummingbird::Engine::load_extensions_from_root(root.path(), &errors);

    EXPECT_TRUE(loaded.empty());
    ASSERT_FALSE(errors.empty());
}
