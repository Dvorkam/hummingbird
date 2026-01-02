#include "core/utils/AssetPath.h"

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>

namespace {
void set_env_var(const char* name, const std::string& value) {
#ifdef _WIN32
    _putenv_s(name, value.c_str());
#else
    setenv(name, value.c_str(), 1);
#endif
}

void unset_env_var(const char* name) {
#ifdef _WIN32
    _putenv_s(name, "");
#else
    unsetenv(name);
#endif
}

class EnvVarGuard {
public:
    EnvVarGuard(const char* name, const std::string& value) : name_(name) {
        if (const char* current = std::getenv(name_)) {
            previous_ = current;
        }
        set_env_var(name_, value);
    }

    ~EnvVarGuard() {
        if (previous_) {
            set_env_var(name_, *previous_);
        } else {
            unset_env_var(name_);
        }
    }

private:
    const char* name_;
    std::optional<std::string> previous_;
};
}  // namespace

TEST(AssetPathTest, ResolvesFontFromRepoRoot) {
    auto path = Hummingbird::resolve_asset_path("assets/fonts/Roboto-Regular.ttf");
    EXPECT_TRUE(std::filesystem::exists(path));
}

TEST(AssetPathTest, RespectsAssetRootEnv) {
    std::filesystem::path root = std::filesystem::temp_directory_path() / "hummingbird-asset-test";
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    std::filesystem::create_directories(root / "assets/fonts");

    std::filesystem::path font_path = root / "assets/fonts/Dummy.ttf";
    std::ofstream(font_path.string()) << "dummy";

    EnvVarGuard guard("HB_ASSET_ROOT", root.string());

    auto resolved = Hummingbird::resolve_asset_path("assets/fonts/Dummy.ttf");
    EXPECT_TRUE(std::filesystem::exists(resolved));
    EXPECT_TRUE(std::filesystem::equivalent(resolved, font_path));

    std::filesystem::remove_all(root, ec);
}
