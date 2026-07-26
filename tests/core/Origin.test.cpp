#include "core/net/Origin.h"

#include <gtest/gtest.h>

using Hummingbird::Core::Origin;

TEST(OriginTest, ParsesSchemeHostAndDefaultPort) {
    auto origin = Origin::parse("https://example.dev/some/path?q=1");
    ASSERT_TRUE(origin.has_value());
    EXPECT_EQ(origin->scheme(), "https");
    EXPECT_EQ(origin->host(), "example.dev");
    EXPECT_EQ(origin->port(), 443);
}

TEST(OriginTest, HttpDefaultsToPortEighty) {
    auto origin = Origin::parse("http://example.dev/");
    ASSERT_TRUE(origin.has_value());
    EXPECT_EQ(origin->port(), 80);
}

TEST(OriginTest, AnExplicitDefaultPortEqualsTheImplicitOne) {
    EXPECT_EQ(Origin::parse("https://example.dev:443/"), Origin::parse("https://example.dev/"));
    EXPECT_EQ(Origin::parse("http://example.dev:80/"), Origin::parse("http://example.dev/"));
}

// The three ways one origin differs from another, each a separate storage bucket.
TEST(OriginTest, SchemeHostAndPortAllDistinguishOrigins) {
    const auto base = Origin::parse("https://example.dev/");
    EXPECT_NE(base, Origin::parse("http://example.dev/"));       // scheme
    EXPECT_NE(base, Origin::parse("https://other.dev/"));        // host
    EXPECT_NE(base, Origin::parse("https://example.dev:8443/"));  // port
    EXPECT_EQ(base, Origin::parse("https://example.dev/other"));  // path is not part of the origin
}

TEST(OriginTest, NonWebAndUnparseableUrlsHaveNoTupleOrigin) {
    // Callers get opaque-origin behavior (no store) by checking the optional.
    EXPECT_FALSE(Origin::parse("file://localhost/C:/x").has_value());
    EXPECT_FALSE(Origin::parse("about:bookmarks").has_value());
    EXPECT_FALSE(Origin::parse("data:text/html,hi").has_value());
    EXPECT_FALSE(Origin::parse("not a url").has_value());
}

TEST(OriginTest, SerializeOmitsTheDefaultPortButKeepsANonDefaultOne) {
    EXPECT_EQ(Origin::parse("https://example.dev/")->serialize(), "https://example.dev");
    EXPECT_EQ(Origin::parse("https://example.dev:8443/")->serialize(), "https://example.dev:8443");
    EXPECT_EQ(Origin::parse("http://example.dev/")->serialize(), "http://example.dev");
}

TEST(OriginTest, KeyIsFilesystemSafeAndDistinguishesEveryComponent) {
    const std::string key = Origin::parse("https://example.dev:8443/")->key();
    EXPECT_EQ(key.find('/'), std::string::npos);
    EXPECT_EQ(key.find(':'), std::string::npos);
    EXPECT_EQ(key.find('\\'), std::string::npos);

    // Same-origin URLs share a key; any component change gives a different one.
    EXPECT_EQ(Origin::parse("https://example.dev/a")->key(), Origin::parse("https://example.dev/b")->key());
    EXPECT_NE(Origin::parse("https://example.dev/")->key(), Origin::parse("http://example.dev/")->key());
    EXPECT_NE(Origin::parse("https://example.dev/")->key(), Origin::parse("https://example.dev:8443/")->key());
}
