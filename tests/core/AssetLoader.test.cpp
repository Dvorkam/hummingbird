#include "core/utils/AssetLoader.h"

#include <gtest/gtest.h>

#include <string>

namespace {
using Hummingbird::Core::Utils::load_asset_bytes;
using Hummingbird::Core::Utils::load_asset_text;
}  // namespace

TEST(AssetLoaderTest, LoadsTextAssets) {
    auto text = load_asset_text("assets/stub.css", true);
    ASSERT_TRUE(text.has_value());
    EXPECT_NE(text->find("background-color"), std::string::npos);
}

TEST(AssetLoaderTest, LoadsBinaryAssets) {
    auto bytes = load_asset_bytes("assets/icons/page_security/secure.png", true);
    ASSERT_TRUE(bytes.has_value());
    EXPECT_GT(bytes->size(), 0u);
}

TEST(AssetLoaderTest, RejectsEmptyAndUrlIds) {
    EXPECT_FALSE(load_asset_text("", false).has_value());
    EXPECT_FALSE(load_asset_text("https://example.dev/style.css", false).has_value());
}

TEST(AssetLoaderTest, MissingAssetReturnsNullopt) {
    EXPECT_FALSE(load_asset_bytes("assets/does_not_exist.bin", false).has_value());
}
