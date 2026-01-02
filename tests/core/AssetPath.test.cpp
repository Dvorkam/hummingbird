#include "core/utils/AssetPath.h"

#include <gtest/gtest.h>

#include <filesystem>

TEST(AssetPathTest, ResolvesFontFromRepoRoot) {
    auto path = Hummingbird::resolve_asset_path("assets/fonts/Roboto-Regular.ttf");
    EXPECT_TRUE(std::filesystem::exists(path));
}
