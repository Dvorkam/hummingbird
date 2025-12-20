#include <gtest/gtest.h>
#include "core/AssetPath.h"
#include <filesystem>

TEST(AssetPathTest, ResolvesFontFromRepoRoot) {
    auto path = Hummingbird::resolve_asset_path("assets/fonts/Roboto-Regular.ttf");
    EXPECT_TRUE(std::filesystem::exists(path));
}
