#include "core/net/IdentityPolicyStore.h"

#include <gtest/gtest.h>

#include <filesystem>

#include "core/net/Origin.h"

using Hummingbird::Core::IdentityMode;
using Hummingbird::Core::IdentityPolicyStore;
using Hummingbird::Core::Origin;

namespace {
Origin origin(std::string_view url) { return *Origin::parse(url); }
}  // namespace

TEST(IdentityPolicyStoreTest, DefaultsToTransparent) {
    IdentityPolicyStore store;
    EXPECT_EQ(store.mode_for(origin("https://news.ycombinator.com/")), IdentityMode::Transparent);
}

TEST(IdentityPolicyStoreTest, SetAndToggle) {
    IdentityPolicyStore store;
    const auto hn = origin("https://news.ycombinator.com/");

    store.set_mode(hn, IdentityMode::Compatibility);
    EXPECT_EQ(store.mode_for(hn), IdentityMode::Compatibility);

    EXPECT_EQ(store.toggle(hn), IdentityMode::Transparent);
    EXPECT_EQ(store.mode_for(hn), IdentityMode::Transparent);
    EXPECT_EQ(store.toggle(hn), IdentityMode::Compatibility);
    EXPECT_EQ(store.mode_for(hn), IdentityMode::Compatibility);
}

TEST(IdentityPolicyStoreTest, ModeIsPerOriginIncludingSchemeAndPort) {
    IdentityPolicyStore store;
    store.set_mode(origin("https://example.dev/"), IdentityMode::Compatibility);
    // Different scheme and different port are different origins.
    EXPECT_EQ(store.mode_for(origin("http://example.dev/")), IdentityMode::Transparent);
    EXPECT_EQ(store.mode_for(origin("https://example.dev:8443/")), IdentityMode::Transparent);
    EXPECT_EQ(store.mode_for(origin("https://example.dev/")), IdentityMode::Compatibility);
}

TEST(IdentityPolicyStoreTest, RoundTripsThroughDisk) {
    auto path = std::filesystem::temp_directory_path() / "hb_identity_test.tsv";
    std::filesystem::remove(path);

    {
        IdentityPolicyStore store;
        store.set_mode(origin("https://news.ycombinator.com/"), IdentityMode::Compatibility);
        store.set_mode(origin("https://example.dev/"), IdentityMode::Compatibility);
        EXPECT_EQ(store.save_to(path), 2u);
    }
    {
        IdentityPolicyStore reloaded;
        EXPECT_EQ(reloaded.load_from(path), 2u);
        EXPECT_EQ(reloaded.mode_for(origin("https://news.ycombinator.com/")), IdentityMode::Compatibility);
        EXPECT_EQ(reloaded.mode_for(origin("https://example.dev/")), IdentityMode::Compatibility);
        EXPECT_EQ(reloaded.mode_for(origin("https://other.test/")), IdentityMode::Transparent);
    }
    std::filesystem::remove(path);
}

TEST(IdentityPolicyStoreTest, SettingBackToTransparentRemovesItFromDisk) {
    auto path = std::filesystem::temp_directory_path() / "hb_identity_test2.tsv";
    std::filesystem::remove(path);

    IdentityPolicyStore store;
    const auto hn = origin("https://news.ycombinator.com/");
    store.set_mode(hn, IdentityMode::Compatibility);
    store.set_mode(hn, IdentityMode::Transparent);
    EXPECT_EQ(store.save_to(path), 0u);

    IdentityPolicyStore reloaded;
    EXPECT_EQ(reloaded.load_from(path), 0u);
    EXPECT_EQ(reloaded.mode_for(hn), IdentityMode::Transparent);
    std::filesystem::remove(path);
}
