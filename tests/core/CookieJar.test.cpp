#include "core/net/CookieJar.h"

#include <gtest/gtest.h>

#include <chrono>
#include <string>

#include "core/net/Cookie.h"
#include "core/net/HttpHeaders.h"

using Hummingbird::Core::Cookie;
using Hummingbird::Core::CookieJar;
using Hummingbird::Core::CookieTime;
using Hummingbird::Core::HttpHeaders;
using Hummingbird::Core::SameSite;

namespace {
// A fixed instant so expiry assertions never depend on the wall clock.
CookieTime now() { return CookieTime{} + std::chrono::hours{24 * 365 * 50}; }
CookieTime later(int seconds) { return now() + std::chrono::seconds{seconds}; }
}  // namespace

// --- parsing -----------------------------------------------------------------

TEST(CookieParseTest, ReadsNameValueAndDefaultsDomainToTheRequestHost) {
    auto cookie = Hummingbird::Core::parse_set_cookie("session=abc123", "https://example.dev/login", now());
    ASSERT_TRUE(cookie.has_value());
    EXPECT_EQ(cookie->name, "session");
    EXPECT_EQ(cookie->value, "abc123");
    EXPECT_EQ(cookie->domain, "example.dev");
    // No Domain attribute => host-only: never sent to a subdomain.
    EXPECT_TRUE(cookie->host_only);
    // No Expires/Max-Age => session cookie.
    EXPECT_TRUE(cookie->is_session());
}

TEST(CookieParseTest, DefaultPathIsTheRequestDirectoryNotTheFullPath) {
    auto cookie = Hummingbird::Core::parse_set_cookie("a=1", "https://example.dev/news/item/42", now());
    ASSERT_TRUE(cookie.has_value());
    EXPECT_EQ(cookie->path, "/news/item");

    auto root = Hummingbird::Core::parse_set_cookie("a=1", "https://example.dev/login", now());
    ASSERT_TRUE(root.has_value());
    EXPECT_EQ(root->path, "/");
}

TEST(CookieParseTest, QueryAndFragmentDoNotLeakIntoTheDefaultPath) {
    // Core's UrlParts::path bundles query and fragment; cookie paths must not.
    auto cookie = Hummingbird::Core::parse_set_cookie("a=1", "https://example.dev/news/item?id=7#top", now());
    ASSERT_TRUE(cookie.has_value());
    EXPECT_EQ(cookie->path, "/news");
}

TEST(CookieParseTest, ReadsAttributesAndStripsALeadingDotFromDomain) {
    auto cookie = Hummingbird::Core::parse_set_cookie(
        "id=7; Domain=.example.dev; Path=/app; Secure; HttpOnly; SameSite=Strict", "https://www.example.dev/x", now());
    ASSERT_TRUE(cookie.has_value());
    EXPECT_EQ(cookie->domain, "example.dev");
    EXPECT_FALSE(cookie->host_only);
    EXPECT_EQ(cookie->path, "/app");
    EXPECT_TRUE(cookie->secure);
    EXPECT_TRUE(cookie->http_only);
    EXPECT_EQ(cookie->same_site, SameSite::Strict);
}

// A page must not be able to set a cookie for a site it is not part of.
TEST(CookieParseTest, RejectsADomainTheRequestHostIsNotWithin) {
    EXPECT_FALSE(
        Hummingbird::Core::parse_set_cookie("evil=1; Domain=example.dev", "https://attacker.test/", now()).has_value());
    // Nor a bare public-suffix-shaped grab from a different registrable domain.
    EXPECT_FALSE(
        Hummingbird::Core::parse_set_cookie("evil=1; Domain=other.dev", "https://example.dev/", now()).has_value());
}

TEST(CookieParseTest, IgnoresAFieldWithNoEqualsOrAnEmptyName) {
    EXPECT_FALSE(Hummingbird::Core::parse_set_cookie("justaflag", "https://example.dev/", now()).has_value());
    EXPECT_FALSE(Hummingbird::Core::parse_set_cookie("=novalue", "https://example.dev/", now()).has_value());
}

TEST(CookieParseTest, MaxAgeOverridesExpires) {
    // RFC 6265 §5.3 step 3: Max-Age wins regardless of attribute order.
    auto cookie = Hummingbird::Core::parse_set_cookie(
        "a=1; Expires=Wed, 09 Jun 2021 10:18:14 GMT; Max-Age=60", "https://example.dev/", now());
    ASSERT_TRUE(cookie.has_value());
    ASSERT_TRUE(cookie->expires.has_value());
    EXPECT_EQ(*cookie->expires, later(60));
}

TEST(CookieParseTest, ParsesBothCommonExpiresSpellings) {
    const auto spaced = Hummingbird::Core::parse_cookie_date("Wed, 09 Jun 2021 10:18:14 GMT");
    const auto dashed = Hummingbird::Core::parse_cookie_date("Wed, 09-Jun-2021 10:18:14 GMT");
    ASSERT_TRUE(spaced.has_value());
    ASSERT_TRUE(dashed.has_value());
    EXPECT_EQ(*spaced, *dashed);
}

TEST(CookieParseTest, RejectsAnUnparseableDate) {
    EXPECT_FALSE(Hummingbird::Core::parse_cookie_date("not a date").has_value());
    EXPECT_FALSE(Hummingbird::Core::parse_cookie_date("").has_value());
    // Missing a time component entirely.
    EXPECT_FALSE(Hummingbird::Core::parse_cookie_date("09 Jun 2021").has_value());
}

// --- matching matrix ---------------------------------------------------------

TEST(CookieMatchTest, DomainMatchCoversHostAndSubdomainsOnly) {
    EXPECT_TRUE(Hummingbird::Core::domain_matches("example.dev", "example.dev"));
    EXPECT_TRUE(Hummingbird::Core::domain_matches("www.example.dev", "example.dev"));
    EXPECT_TRUE(Hummingbird::Core::domain_matches("a.b.example.dev", "example.dev"));
    // A suffix that is not on a label boundary is not a subdomain.
    EXPECT_FALSE(Hummingbird::Core::domain_matches("notexample.dev", "example.dev"));
    EXPECT_FALSE(Hummingbird::Core::domain_matches("example.dev", "www.example.dev"));
    // An IP literal only matches itself.
    EXPECT_TRUE(Hummingbird::Core::domain_matches("127.0.0.1", "127.0.0.1"));
    EXPECT_FALSE(Hummingbird::Core::domain_matches("127.0.0.1", "0.0.1"));
}

TEST(CookieMatchTest, PathMatchRequiresALabelBoundary) {
    EXPECT_TRUE(Hummingbird::Core::path_matches("/app", "/app"));
    EXPECT_TRUE(Hummingbird::Core::path_matches("/app/inner", "/app"));
    EXPECT_TRUE(Hummingbird::Core::path_matches("/app/inner", "/app/"));
    EXPECT_TRUE(Hummingbird::Core::path_matches("/anything", "/"));
    // "/app" must not cover "/application".
    EXPECT_FALSE(Hummingbird::Core::path_matches("/application", "/app"));
    EXPECT_FALSE(Hummingbird::Core::path_matches("/", "/app"));
}

// --- jar behavior ------------------------------------------------------------

TEST(CookieJarTest, ServerSetCookieComesBackOnTheNextMatchingRequest) {
    CookieJar jar;
    ASSERT_TRUE(jar.store_from_header("https://example.dev/login", "session=abc; Path=/", now()));

    EXPECT_EQ(jar.cookie_header_for("https://example.dev/", now()), "session=abc");
    EXPECT_EQ(jar.cookie_header_for("https://example.dev/deep/page", now()), "session=abc");
}

TEST(CookieJarTest, CookieIsNotSentToANonMatchingDomainOrPath) {
    CookieJar jar;
    ASSERT_TRUE(jar.store_from_header("https://example.dev/app/x", "scoped=1; Path=/app", now()));

    EXPECT_EQ(jar.cookie_header_for("https://example.dev/app/inner", now()), "scoped=1");
    EXPECT_TRUE(jar.cookie_header_for("https://example.dev/other", now()).empty());
    EXPECT_TRUE(jar.cookie_header_for("https://other.test/app", now()).empty());
}

TEST(CookieJarTest, HostOnlyCookieDoesNotTravelToSubdomains) {
    CookieJar jar;
    // No Domain attribute => host-only.
    ASSERT_TRUE(jar.store_from_header("https://example.dev/", "a=1", now()));
    // With Domain => shared with subdomains.
    ASSERT_TRUE(jar.store_from_header("https://example.dev/", "b=2; Domain=example.dev", now()));

    EXPECT_EQ(jar.cookie_header_for("https://example.dev/", now()), "a=1; b=2");
    EXPECT_EQ(jar.cookie_header_for("https://www.example.dev/", now()), "b=2");
}

TEST(CookieJarTest, ExpiredCookieIsNeitherSentNorRetained) {
    CookieJar jar;
    ASSERT_TRUE(jar.store_from_header("https://example.dev/", "short=1; Max-Age=30", now()));
    EXPECT_EQ(jar.cookie_header_for("https://example.dev/", now()), "short=1");

    // Past its expiry it stops matching...
    EXPECT_TRUE(jar.cookie_header_for("https://example.dev/", later(31)).empty());
    // ...and a purge reclaims it.
    EXPECT_EQ(jar.purge_expired(later(31)), 1u);
    EXPECT_TRUE(jar.empty());
}

TEST(CookieJarTest, MaxAgeZeroDeletesAStoredCookie) {
    CookieJar jar;
    ASSERT_TRUE(jar.store_from_header("https://example.dev/", "session=abc", now()));
    ASSERT_EQ(jar.size(), 1u);

    // How a server logs you out.
    ASSERT_TRUE(jar.store_from_header("https://example.dev/", "session=; Max-Age=0", now()));
    EXPECT_TRUE(jar.empty());
}

TEST(CookieJarTest, ResettingASameIdentityCookieReplacesRatherThanDuplicates) {
    CookieJar jar;
    ASSERT_TRUE(jar.store_from_header("https://example.dev/", "session=old", now()));
    ASSERT_TRUE(jar.store_from_header("https://example.dev/", "session=new", now()));

    EXPECT_EQ(jar.size(), 1u);
    EXPECT_EQ(jar.cookie_header_for("https://example.dev/", now()), "session=new");
}

TEST(CookieJarTest, SecureCookieIsWithheldFromPlainHttp) {
    CookieJar jar;
    ASSERT_TRUE(jar.store_from_header("https://example.dev/", "secure_id=1; Secure", now()));
    ASSERT_TRUE(jar.store_from_header("https://example.dev/", "plain=2", now()));

    EXPECT_EQ(jar.cookie_header_for("https://example.dev/", now()), "secure_id=1; plain=2");
    EXPECT_EQ(jar.cookie_header_for("http://example.dev/", now()), "plain=2");
}

TEST(CookieJarTest, SendOrderPutsLongerPathsFirst) {
    CookieJar jar;
    ASSERT_TRUE(jar.store_from_header("https://example.dev/", "root=1; Path=/", now()));
    ASSERT_TRUE(jar.store_from_header("https://example.dev/", "deep=2; Path=/app/inner", now()));
    ASSERT_TRUE(jar.store_from_header("https://example.dev/", "mid=3; Path=/app", now()));

    EXPECT_EQ(jar.cookie_header_for("https://example.dev/app/inner/page", now()), "deep=2; mid=3; root=1");
}

TEST(CookieJarTest, StoresEverySetCookieFieldOfAResponse) {
    CookieJar jar;
    HttpHeaders headers;
    headers.add("Set-Cookie", "session=abc; Path=/");
    headers.add("Set-Cookie", "theme=dark");
    headers.add("Content-Type", "text/html");

    EXPECT_EQ(jar.store_from_response("https://example.dev/login", headers, now()), 2u);
    EXPECT_EQ(jar.cookie_header_for("https://example.dev/", now()), "session=abc; theme=dark");
}

TEST(CookieJarTest, AMalformedCookieIsSkippedWithoutLosingTheRest) {
    CookieJar jar;
    HttpHeaders headers;
    headers.add("Set-Cookie", "justaflag");                          // no '='
    headers.add("Set-Cookie", "evil=1; Domain=attacker.test");       // not our domain
    headers.add("Set-Cookie", "good=2");

    EXPECT_EQ(jar.store_from_response("https://example.dev/", headers, now()), 1u);
    EXPECT_EQ(jar.cookie_header_for("https://example.dev/", now()), "good=2");
}

TEST(CookieJarTest, NoMatchYieldsAnEmptyHeaderNotAStrayDelimiter) {
    CookieJar jar;
    EXPECT_TRUE(jar.cookie_header_for("https://example.dev/", now()).empty());
    EXPECT_TRUE(jar.cookie_header_for("not a url", now()).empty());
}
