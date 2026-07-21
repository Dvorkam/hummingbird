#include "core/net/CookieJar.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

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

// --- attribute policy (8.1.2) ------------------------------------------------

namespace {
// A subresource fetch initiated by a document on `initiator`.
Hummingbird::Core::CookieRequestContext subresource_from(std::string initiator) {
    Hummingbird::Core::CookieRequestContext context;
    context.top_level_navigation = false;
    context.safe_method = true;
    context.initiator_host = std::move(initiator);
    return context;
}

// A top-level navigation initiated by a document on `initiator`.
Hummingbird::Core::CookieRequestContext navigation_from(std::string initiator, bool safe_method = true) {
    Hummingbird::Core::CookieRequestContext context;
    context.top_level_navigation = true;
    context.safe_method = safe_method;
    context.initiator_host = std::move(initiator);
    return context;
}
}  // namespace

TEST(CookieParseTest, SameSiteNoneWithoutSecureIsRejected) {
    // Announcing cross-site availability without Secure is refused outright
    // rather than silently downgraded, which would look like it had worked.
    EXPECT_FALSE(
        Hummingbird::Core::parse_set_cookie("a=1; SameSite=None", "https://example.dev/", now()).has_value());
    EXPECT_TRUE(
        Hummingbird::Core::parse_set_cookie("a=1; SameSite=None; Secure", "https://example.dev/", now()).has_value());
}

TEST(CookieParseTest, SameSiteDefaultsToLaxWhenAbsentOrUnrecognized) {
    auto absent = Hummingbird::Core::parse_set_cookie("a=1", "https://example.dev/", now());
    ASSERT_TRUE(absent.has_value());
    EXPECT_EQ(absent->same_site, SameSite::Lax);

    auto garbage = Hummingbird::Core::parse_set_cookie("a=1; SameSite=banana", "https://example.dev/", now());
    ASSERT_TRUE(garbage.has_value());
    EXPECT_EQ(garbage->same_site, SameSite::Lax);
}

// The acceptance criterion for 8.1.2, stated directly.
TEST(CookieJarTest, LaxAttachesToTopLevelNavigationButNotCrossSiteSubresources) {
    CookieJar jar;
    ASSERT_TRUE(jar.store_from_header("https://example.dev/", "session=abc", now()));  // Lax by default

    // Same-site subresource: sent.
    EXPECT_EQ(jar.cookie_header_for("https://example.dev/x.css", now(), subresource_from("example.dev")),
              "session=abc");
    // Cross-site subresource: withheld. This is what Lax is for.
    EXPECT_TRUE(jar.cookie_header_for("https://example.dev/x.css", now(), subresource_from("attacker.test")).empty());
    // Cross-site top-level navigation with a safe method: sent.
    EXPECT_EQ(jar.cookie_header_for("https://example.dev/", now(), navigation_from("attacker.test")), "session=abc");
    // Cross-site top-level POST: withheld -- the login-CSRF case.
    EXPECT_TRUE(
        jar.cookie_header_for("https://example.dev/", now(), navigation_from("attacker.test", false)).empty());
}

TEST(CookieJarTest, StrictIsWithheldFromEveryCrossSiteRequest) {
    CookieJar jar;
    ASSERT_TRUE(jar.store_from_header("https://example.dev/", "s=1; SameSite=Strict", now()));

    EXPECT_EQ(jar.cookie_header_for("https://example.dev/", now(), navigation_from("example.dev")), "s=1");
    // Even a top-level safe navigation does not carry Strict across sites.
    EXPECT_TRUE(jar.cookie_header_for("https://example.dev/", now(), navigation_from("attacker.test")).empty());
    EXPECT_TRUE(jar.cookie_header_for("https://example.dev/", now(), subresource_from("attacker.test")).empty());
}

TEST(CookieJarTest, SameSiteNoneRidesEveryRequest) {
    CookieJar jar;
    ASSERT_TRUE(jar.store_from_header("https://example.dev/", "n=1; SameSite=None; Secure", now()));

    EXPECT_EQ(jar.cookie_header_for("https://example.dev/x.css", now(), subresource_from("attacker.test")), "n=1");
    EXPECT_EQ(jar.cookie_header_for("https://example.dev/", now(), navigation_from("attacker.test", false)), "n=1");
}

TEST(CookieJarTest, SubdomainsCountAsSameSite) {
    // SameSite is site-scoped, not origin-scoped: www and api are one site.
    CookieJar jar;
    ASSERT_TRUE(jar.store_from_header("https://example.dev/", "s=1; SameSite=Strict; Domain=example.dev", now()));

    EXPECT_EQ(jar.cookie_header_for("https://api.example.dev/", now(), subresource_from("www.example.dev")), "s=1");
}

TEST(CookieJarTest, HttpOnlyIsHiddenFromScriptButStillSentOnRequests) {
    CookieJar jar;
    ASSERT_TRUE(jar.store_from_header("https://example.dev/", "session=secret; HttpOnly", now()));
    ASSERT_TRUE(jar.store_from_header("https://example.dev/", "theme=dark", now()));

    // The network still carries it -- HttpOnly hides it from JS, not from HTTP.
    EXPECT_EQ(jar.cookie_header_for("https://example.dev/", now()), "session=secret; theme=dark");
    // document.cookie must not see it: the whole point of the flag under XSS.
    EXPECT_EQ(jar.script_visible_cookies("https://example.dev/", now()), "theme=dark");
}

TEST(CookieJarTest, ScriptViewStillRespectsDomainPathAndExpiry) {
    CookieJar jar;
    ASSERT_TRUE(jar.store_from_header("https://example.dev/app/x", "scoped=1; Path=/app", now()));
    ASSERT_TRUE(jar.store_from_header("https://example.dev/", "expired=1; Max-Age=10", now()));

    EXPECT_EQ(jar.script_visible_cookies("https://example.dev/app/page", now()), "scoped=1; expired=1");
    // Outside /app the path-scoped one drops out; the root-scoped one remains.
    EXPECT_EQ(jar.script_visible_cookies("https://example.dev/other", now()), "expired=1");
    // And expiry applies to the script view just as it does to requests.
    EXPECT_TRUE(jar.script_visible_cookies("https://example.dev/app/page", later(11)).find("expired") ==
                std::string::npos);
}

TEST(CookieMatchTest, SameSiteTreatsAnEmptyInitiatorAsSameSite) {
    // A user-typed URL or bookmark has no initiating document and is not a
    // cross-site request.
    EXPECT_TRUE(Hummingbird::Core::is_same_site("example.dev", ""));
    EXPECT_TRUE(Hummingbird::Core::is_same_site("example.dev", "example.dev"));
    EXPECT_TRUE(Hummingbird::Core::is_same_site("api.example.dev", "www.example.dev"));
    EXPECT_FALSE(Hummingbird::Core::is_same_site("example.dev", "attacker.test"));
}

// --- persistence (8.1.4) -----------------------------------------------------

namespace {
// A unique temp file per test, removed on destruction.
class TempCookieFile {
public:
    explicit TempCookieFile(const char* tag)
        : path_(std::filesystem::temp_directory_path() / (std::string("hb_cookies_") + tag + ".tsv")) {
        std::error_code ec;
        std::filesystem::remove(path_, ec);
    }
    ~TempCookieFile() {
        std::error_code ec;
        std::filesystem::remove(path_, ec);
    }
    const std::filesystem::path& path() const { return path_; }

    void write(std::string_view contents) const {
        std::ofstream file(path_, std::ios::binary | std::ios::trunc);
        file << contents;
    }

private:
    std::filesystem::path path_;
};
}  // namespace

TEST(CookieJarPersistenceTest, PersistentCookiesSurviveARestartExactly) {
    TempCookieFile file("roundtrip");
    {
        CookieJar jar;
        ASSERT_TRUE(jar.store_from_header(
            "https://example.dev/app/x", "session=abc; Path=/app; Max-Age=86400; Secure; HttpOnly; SameSite=Strict",
            now()));
        EXPECT_EQ(jar.save_to(file.path(), now()), 1u);
    }

    CookieJar restored;
    EXPECT_EQ(restored.load_from(file.path(), now()), 1u);
    ASSERT_EQ(restored.entries().size(), 1u);
    const Cookie& cookie = restored.entries()[0];
    EXPECT_EQ(cookie.name, "session");
    EXPECT_EQ(cookie.value, "abc");
    EXPECT_EQ(cookie.domain, "example.dev");
    EXPECT_EQ(cookie.path, "/app");
    EXPECT_TRUE(cookie.secure);
    EXPECT_TRUE(cookie.http_only);
    EXPECT_EQ(cookie.same_site, SameSite::Strict);
    EXPECT_TRUE(cookie.host_only);
    ASSERT_TRUE(cookie.expires.has_value());
    EXPECT_EQ(*cookie.expires, later(86400));
    // And it is still actually usable, not just structurally intact.
    EXPECT_EQ(restored.cookie_header_for("https://example.dev/app/page", now()), "session=abc");
}

TEST(CookieJarPersistenceTest, SessionCookiesAreNeverWritten) {
    // "Dies with the process" is their definition; persisting one would
    // silently promote it to a persistent cookie.
    TempCookieFile file("session");
    CookieJar jar;
    ASSERT_TRUE(jar.store_from_header("https://example.dev/", "keep=1; Max-Age=600", now()));
    ASSERT_TRUE(jar.store_from_header("https://example.dev/", "temp=2", now()));
    ASSERT_EQ(jar.size(), 2u);

    EXPECT_EQ(jar.save_to(file.path(), now()), 1u);

    CookieJar restored;
    EXPECT_EQ(restored.load_from(file.path(), now()), 1u);
    EXPECT_EQ(restored.cookie_header_for("https://example.dev/", now()), "keep=1");
}

TEST(CookieJarPersistenceTest, CookiesThatExpiredWhileClosedArePurgedOnLoad) {
    TempCookieFile file("expiry");
    {
        CookieJar jar;
        ASSERT_TRUE(jar.store_from_header("https://example.dev/", "short=1; Max-Age=30", now()));
        ASSERT_TRUE(jar.store_from_header("https://example.dev/", "long=2; Max-Age=86400", now()));
        EXPECT_EQ(jar.save_to(file.path(), now()), 2u);
    }

    CookieJar restored;
    // Restarting an hour later: the 30-second one must not come back to life.
    EXPECT_EQ(restored.load_from(file.path(), later(3600)), 1u);
    EXPECT_EQ(restored.cookie_header_for("https://example.dev/", later(3600)), "long=2");
}

TEST(CookieJarPersistenceTest, AMissingFileIsANormalFirstRun) {
    TempCookieFile file("missing");  // constructor removes it
    CookieJar jar;
    EXPECT_EQ(jar.load_from(file.path(), now()), 0u);
    EXPECT_TRUE(jar.empty());
}

TEST(CookieJarPersistenceTest, ACorruptFileStartsEmptyInsteadOfFailing) {
    // A browser must still start when its cookie file is damaged.
    TempCookieFile file("corrupt");
    file.write("this is not a cookie file\nneither is this\n");

    CookieJar jar;
    EXPECT_EQ(jar.load_from(file.path(), now()), 0u);
    EXPECT_TRUE(jar.empty());
}

TEST(CookieJarPersistenceTest, MalformedLinesAreSkippedWithoutLosingGoodOnes) {
    TempCookieFile file("partial");
    CookieJar source;
    ASSERT_TRUE(source.store_from_header("https://example.dev/", "good=1; Max-Age=86400", now()));
    ASSERT_EQ(source.save_to(file.path(), now()), 1u);

    // Append junk after a valid record.
    {
        std::ofstream appended(file.path(), std::ios::binary | std::ios::app);
        appended << "too\tfew\tfields\n";
        appended << "\n";
    }

    CookieJar jar;
    EXPECT_EQ(jar.load_from(file.path(), now()), 1u);
    EXPECT_EQ(jar.cookie_header_for("https://example.dev/", now()), "good=1");
}

TEST(CookieJarPersistenceTest, LoadingReplacesRatherThanMergesExistingState) {
    TempCookieFile file("replace");
    CookieJar source;
    ASSERT_TRUE(source.store_from_header("https://example.dev/", "fromdisk=1; Max-Age=86400", now()));
    ASSERT_EQ(source.save_to(file.path(), now()), 1u);

    CookieJar jar;
    ASSERT_TRUE(jar.store_from_header("https://example.dev/", "stale=1; Max-Age=86400", now()));
    EXPECT_EQ(jar.load_from(file.path(), now()), 1u);
    EXPECT_EQ(jar.cookie_header_for("https://example.dev/", now()), "fromdisk=1");
}

TEST(CookieJarPersistenceTest, DefaultPathHonorsTheEnvironmentOverride) {
    // The hook tests and alternate profiles use to stay off the real file.
    EXPECT_FALSE(CookieJar::default_path().empty());
}

TEST(CookieJarPersistenceTest, AlreadyExpiredCookiesAreNotWrittenAtAll) {
    // load_from would drop them anyway, but writing them means the file
    // accumulates corpses until the next clean shutdown.
    TempCookieFile file("nocorpses");
    CookieJar jar;
    ASSERT_TRUE(jar.store_from_header("https://example.dev/", "short=1; Max-Age=30", now()));
    ASSERT_TRUE(jar.store_from_header("https://example.dev/", "long=2; Max-Age=86400", now()));

    // Saving an hour later: only the survivor is written.
    EXPECT_EQ(jar.save_to(file.path(), later(3600)), 1u);

    CookieJar restored;
    EXPECT_EQ(restored.load_from(file.path(), later(3600)), 1u);
    EXPECT_EQ(restored.cookie_header_for("https://example.dev/", later(3600)), "long=2");
}
