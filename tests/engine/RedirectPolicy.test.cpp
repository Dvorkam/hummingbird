#include "engine/resources/RedirectPolicy.h"

#include <gtest/gtest.h>

namespace Policy = Hummingbird::Engine::RedirectPolicy;

TEST(RedirectPolicyTest, RecognizesTheFollowableStatuses) {
    for (long status : {301L, 302L, 303L, 307L, 308L}) {
        EXPECT_TRUE(Policy::is_redirect_status(status)) << status;
    }
    for (long status : {200L, 204L, 300L, 304L, 400L, 404L, 500L}) {
        EXPECT_FALSE(Policy::is_redirect_status(status)) << status;
    }
}

// RFC 9110 §15.4 method semantics -- the matrix the story names.
TEST(RedirectPolicyTest, OnlyThreeOhSevenAndEightPreserveThePostMethod) {
    EXPECT_TRUE(Policy::preserves_method(307));
    EXPECT_TRUE(Policy::preserves_method(308));
    // 303 mandates the rewrite; 301/302 are rewritten because every browser
    // does and sites depend on it (a deliberate deviation from the letter).
    EXPECT_FALSE(Policy::preserves_method(303));
    EXPECT_FALSE(Policy::preserves_method(301));
    EXPECT_FALSE(Policy::preserves_method(302));
}

TEST(RedirectPolicyTest, PostSurvivesA307ButIsRewrittenByA302) {
    auto preserved = Policy::decide(307, "/next", "https://example.test/form", /*was_post*/ true);
    ASSERT_TRUE(preserved.has_value());
    EXPECT_EQ(preserved->url, "https://example.test/next");
    EXPECT_TRUE(preserved->keep_post);

    auto rewritten = Policy::decide(302, "/next", "https://example.test/form", /*was_post*/ true);
    ASSERT_TRUE(rewritten.has_value());
    EXPECT_FALSE(rewritten->keep_post) << "POST -> 302 -> GET is what reference browsers do";
}

TEST(RedirectPolicyTest, AGetNeverBecomesAPost) {
    auto decision = Policy::decide(307, "/next", "https://example.test/page", /*was_post*/ false);
    ASSERT_TRUE(decision.has_value());
    EXPECT_FALSE(decision->keep_post);
}

TEST(RedirectPolicyTest, ResolvesRelativeAbsoluteAndProtocolRelativeTargets) {
    auto rooted = Policy::decide(302, "/login", "https://example.test/deep/page", false);
    ASSERT_TRUE(rooted.has_value());
    EXPECT_EQ(rooted->url, "https://example.test/login");

    auto relative = Policy::decide(302, "next", "https://example.test/deep/page", false);
    ASSERT_TRUE(relative.has_value());
    EXPECT_EQ(relative->url, "https://example.test/deep/next");

    auto absolute = Policy::decide(302, "https://other.test/x", "https://example.test/", false);
    ASSERT_TRUE(absolute.has_value());
    EXPECT_EQ(absolute->url, "https://other.test/x");
}

TEST(RedirectPolicyTest, IgnoresWhitespaceAroundLocation) {
    auto decision = Policy::decide(302, "  /login\r\n", "https://example.test/", false);
    ASSERT_TRUE(decision.has_value());
    EXPECT_EQ(decision->url, "https://example.test/login");
}

TEST(RedirectPolicyTest, AThreeXxWithoutAUsableLocationIsNotFollowed) {
    // Delivered to the caller as-is, like a browser would.
    EXPECT_FALSE(Policy::decide(302, "", "https://example.test/", false).has_value());
    EXPECT_FALSE(Policy::decide(302, "   ", "https://example.test/", false).has_value());
}

TEST(RedirectPolicyTest, RefusesToRedirectIntoAScriptUrl) {
    // Redirect-to-script is an injection vector, not a navigation.
    EXPECT_FALSE(Policy::decide(302, "javascript:alert(1)", "https://example.test/", false).has_value());
}

TEST(RedirectPolicyTest, NonRedirectStatusesYieldNoDecision) {
    EXPECT_FALSE(Policy::decide(200, "/elsewhere", "https://example.test/", false).has_value());
    EXPECT_FALSE(Policy::decide(404, "/elsewhere", "https://example.test/", false).has_value());
}
