#include "core/net/Referrer.h"

#include <gtest/gtest.h>

using Hummingbird::Core::compute_referrer_header;

// Same origin: the full URL, minus the fragment. This is the case HN's comment
// POST depends on — item page and /comment share an origin.
TEST(ReferrerTest, SameOriginSendsFullUrlWithoutFragment) {
    auto referer = compute_referrer_header("https://news.ycombinator.com/item?id=42#c1",
                                           "https://news.ycombinator.com/comment");
    ASSERT_TRUE(referer.has_value());
    EXPECT_EQ(*referer, "https://news.ycombinator.com/item?id=42");
}

// Same origin includes the query string (only the fragment is stripped).
TEST(ReferrerTest, SameOriginKeepsQuery) {
    auto referer = compute_referrer_header("https://example.dev/a?x=1", "https://example.dev/b");
    ASSERT_TRUE(referer.has_value());
    EXPECT_EQ(*referer, "https://example.dev/a?x=1");
}

// Cross origin: origin only, no path — the strict-origin-when-cross-origin
// default. The path of the source page is not leaked to another site.
TEST(ReferrerTest, CrossOriginSendsOriginOnly) {
    auto referer = compute_referrer_header("https://example.dev/secret/path", "https://other.dev/x");
    ASSERT_TRUE(referer.has_value());
    EXPECT_EQ(*referer, "https://example.dev/");
}

// A different port is a different origin, so it degrades to origin-only too, and
// the port is preserved in the emitted origin.
TEST(ReferrerTest, DifferentPortIsCrossOrigin) {
    auto referer = compute_referrer_header("https://example.dev:8443/page", "https://example.dev/x");
    ASSERT_TRUE(referer.has_value());
    EXPECT_EQ(*referer, "https://example.dev:8443/");
}

// TLS downgrade: a secure page must never reveal even its origin to an insecure
// destination.
TEST(ReferrerTest, HttpsToHttpSendsNothing) {
    EXPECT_FALSE(compute_referrer_header("https://example.dev/page", "http://example.dev/x").has_value());
}

// http -> https is not a downgrade; it degrades to origin-only (cross origin by
// scheme) but is still sent.
TEST(ReferrerTest, HttpToHttpsSendsOrigin) {
    auto referer = compute_referrer_header("http://example.dev/page", "https://example.dev/x");
    ASSERT_TRUE(referer.has_value());
    EXPECT_EQ(*referer, "http://example.dev/");
}

// A user-initiated navigation (address bar / bookmark / history) has no
// initiating document, so it carries no referrer.
TEST(ReferrerTest, EmptySourceSendsNothing) {
    EXPECT_FALSE(compute_referrer_header("", "https://example.dev/x").has_value());
}

// A source with no tuple origin (non-web scheme) cannot be a referrer.
TEST(ReferrerTest, NonWebSourceSendsNothing) {
    EXPECT_FALSE(compute_referrer_header("about:bookmarks", "https://example.dev/x").has_value());
    EXPECT_FALSE(compute_referrer_header("file:///C:/secret.txt", "https://example.dev/x").has_value());
}
