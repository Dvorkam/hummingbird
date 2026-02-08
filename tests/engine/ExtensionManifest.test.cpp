#include <gtest/gtest.h>

#include <string_view>

#include "engine/extensions/ExtensionManifest.h"

namespace {
using Hummingbird::Engine::ManifestParseError;
using Hummingbird::Engine::parse_extension_manifest;
}  // namespace

TEST(ExtensionManifestTest, ParsesMinimalValidManifest) {
    constexpr std::string_view json = R"JSON(
{
  "name": "Dark Mode",
  "version": "0.1.0",
  "background": { "entry": "background.js" }
}
)JSON";

    ManifestParseError error;
    auto manifest = parse_extension_manifest(json, &error);
    ASSERT_TRUE(manifest.has_value()) << error.message;
    EXPECT_EQ(manifest->name, "Dark Mode");
    EXPECT_EQ(manifest->version, "0.1.0");
    EXPECT_EQ(manifest->background_entry, "background.js");
    EXPECT_TRUE(manifest->permissions.empty());
}

TEST(ExtensionManifestTest, ParsesPermissionsArray) {
    constexpr std::string_view json = R"JSON(
{
  "name": "Dark Mode",
  "version": "0.1.0",
  "background": { "entry": "background.js" },
  "permissions": ["tabs", "scripting"]
}
)JSON";

    ManifestParseError error;
    auto manifest = parse_extension_manifest(json, &error);
    ASSERT_TRUE(manifest.has_value()) << error.message;
    ASSERT_EQ(manifest->permissions.size(), 2u);
    EXPECT_EQ(manifest->permissions[0], "tabs");
    EXPECT_EQ(manifest->permissions[1], "scripting");
}

TEST(ExtensionManifestTest, RejectsMissingRequiredFields) {
    constexpr std::string_view json = R"JSON(
{
  "name": "Missing Background",
  "version": "0.1.0"
}
)JSON";

    ManifestParseError error;
    auto manifest = parse_extension_manifest(json, &error);
    EXPECT_FALSE(manifest.has_value());
    EXPECT_EQ(error.message, "Missing required field: background.entry");
}

TEST(ExtensionManifestTest, RejectsWrongTypeForBackground) {
    constexpr std::string_view json = R"JSON(
{
  "name": "Bad",
  "version": "0.1.0",
  "background": "background.js"
}
)JSON";

    ManifestParseError error;
    auto manifest = parse_extension_manifest(json, &error);
    EXPECT_FALSE(manifest.has_value());
    EXPECT_EQ(error.message, "background must be an object");
}

TEST(ExtensionManifestTest, RejectsNonStringPermissions) {
    constexpr std::string_view json = R"JSON(
{
  "name": "Bad",
  "version": "0.1.0",
  "background": { "entry": "background.js" },
  "permissions": ["tabs", 1]
}
)JSON";

    ManifestParseError error;
    auto manifest = parse_extension_manifest(json, &error);
    EXPECT_FALSE(manifest.has_value());
    EXPECT_EQ(error.message, "Expected string");
}

