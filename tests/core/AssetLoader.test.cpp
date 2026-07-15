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

TEST(AssetLoaderTest, RejectsNonRelativePaths) {
    // Page-controlled URLs must never reach the filesystem as absolute, UNC/SMB,
    // drive, or traversal paths (T-SEC-URL-1).
    EXPECT_FALSE(load_asset_bytes("//evil.example/share/x", false).has_value());      // protocol-relative / UNC
    EXPECT_FALSE(load_asset_bytes("\\\\evil.example\\share\\x", false).has_value());  // Windows UNC
    EXPECT_FALSE(load_asset_bytes("/etc/passwd", false).has_value());                 // absolute
    EXPECT_FALSE(load_asset_bytes("C:\\Windows\\win.ini", false).has_value());        // drive-absolute
    EXPECT_FALSE(load_asset_bytes("../../secret.key", false).has_value());            // parent traversal
    EXPECT_FALSE(load_asset_text("assets/../../secret", false).has_value());          // embedded traversal
}

TEST(AssetLoaderTest, MissingAssetReturnsNullopt) {
    EXPECT_FALSE(load_asset_bytes("assets/does_not_exist.bin", false).has_value());
}
