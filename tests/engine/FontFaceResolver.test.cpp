#include "engine/document/DocumentResources.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iterator>

#include "engine/resources/ResourceStore.h"
#include "style/compute/Stylesheet.h"

using Hummingbird::Engine::DocumentResources;
using Hummingbird::Engine::ResourceStore;
using Hummingbird::Engine::ResourceType;

namespace {
Hummingbird::Css::FontFaceRule make_face(std::string family,
                                         std::vector<Hummingbird::Css::FontFaceSource> sources) {
    Hummingbird::Css::FontFaceRule face;
    face.family = std::move(family);
    face.sources = std::move(sources);
    return face;
}
}  // namespace

TEST(FontFaceResolverTest, RegistersLocalAssetSourceImmediately) {
    ResourceStore store;
    DocumentResources resources(&store, nullptr, nullptr);

    std::vector<std::string> pending;
    auto registry = resources.resolve_font_faces({make_face("myicons", {{"fonts/icons.ttf", "truetype"}})}, pending);

    EXPECT_TRUE(pending.empty());
    EXPECT_EQ(registry.resolve("myicons"), "fonts/icons.ttf");
}

TEST(FontFaceResolverTest, SkipsFacesWithOnlyUndecodableSources) {
    ResourceStore store;
    DocumentResources resources(&store, nullptr, nullptr);

    std::vector<std::string> pending;
    auto registry =
        resources.resolve_font_faces({make_face("icons", {{"https://cdn.test/icons.woff2", "woff2"}})}, pending);

    // WOFF2 has no decoder yet: nothing registered, nothing requested.
    EXPECT_TRUE(pending.empty());
    EXPECT_TRUE(registry.resolve("icons").empty());
}

TEST(FontFaceResolverTest, RequestsRemoteFontNotYetFetched) {
    ResourceStore store;
    DocumentResources resources(&store, nullptr, nullptr);

    std::vector<std::string> pending;
    auto registry =
        resources.resolve_font_faces({make_face("remote", {{"https://cdn.test/font.ttf", "truetype"}})}, pending);

    ASSERT_EQ(pending.size(), 1u);
    EXPECT_EQ(pending[0], "https://cdn.test/font.ttf");
    EXPECT_TRUE(registry.resolve("remote").empty());
}

TEST(FontFaceResolverTest, PrefersDecodableSourceOverWoff2) {
    ResourceStore store;
    DocumentResources resources(&store, nullptr, nullptr);

    std::vector<std::string> pending;
    auto registry = resources.resolve_font_faces(
        {make_face("multi", {{"https://cdn.test/font.woff2", "woff2"}, {"https://cdn.test/font.ttf", "truetype"}})},
        pending);

    // The woff2 is skipped; the ttf (loadable but not yet fetched) is requested.
    ASSERT_EQ(pending.size(), 1u);
    EXPECT_EQ(pending[0], "https://cdn.test/font.ttf");
}

TEST(FontFaceResolverTest, WritesReadyRemoteFontToDiskCacheAndRegistersPath) {
    ResourceStore store;
    const std::string url = "https://cdn.test/ready-font.ttf";
    // Explicit length so the embedded NUL bytes are preserved (binary font data).
    const std::string bytes("\x00\x01\x02TTF-BYTES", 12);
    store.begin_request(url, ResourceType::Font);
    store.mark_ready(url, ResourceType::Font, bytes);

    DocumentResources resources(&store, nullptr, nullptr);
    std::vector<std::string> pending;
    auto registry = resources.resolve_font_faces({make_face("ready", {{url, "truetype"}})}, pending);

    EXPECT_TRUE(pending.empty());
    std::string key = registry.resolve("ready");
    ASSERT_FALSE(key.empty());

    std::filesystem::path cached(key);
    ASSERT_TRUE(std::filesystem::exists(cached));
    std::ifstream in(cached, std::ios::binary);
    std::string written((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    EXPECT_EQ(written, bytes);

    std::error_code ec;
    std::filesystem::remove(cached, ec);
}
