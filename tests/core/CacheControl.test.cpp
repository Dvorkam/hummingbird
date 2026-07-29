// Story 9.3.1: the HTTP cache's policy layer. Pure functions over headers, so
// the whole matrix is checkable without a server or a store.
//
// Several cases here use the literal header values observed on the M9 proof
// endpoints (api.hnpwa.com and en.wikipedia.org, probed 2026-07-29) rather than
// invented ones. A cache is exactly the kind of code that passes against tidy
// fixtures and then misreads what real CDNs send.
#include "core/net/CacheControl.h"

#include <gtest/gtest.h>

#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

#include "core/net/Cookie.h"

namespace {
using Hummingbird::Core::CacheTime;
using Hummingbird::Core::compute_freshness;
using Hummingbird::Core::credentials_class;
using Hummingbird::Core::CredentialsClass;
using Hummingbird::Core::extract_validators;
using Hummingbird::Core::HttpHeaders;
using Hummingbird::Core::parse_cache_control;
using Hummingbird::Core::Storability;
using Hummingbird::Core::storability;

// A fixed instant to measure everything against, so no test depends on the
// wall clock. Chosen to match the `Date` values used below.
CacheTime at(const char* http_date) {
    const auto parsed = Hummingbird::Core::parse_cookie_date(http_date);
    EXPECT_TRUE(parsed.has_value()) << "test fixture date did not parse: " << http_date;
    return parsed.value_or(CacheTime{});
}

HttpHeaders headers_of(std::initializer_list<std::pair<const char*, const char*>> fields) {
    HttpHeaders headers;
    for (const auto& [name, value] : fields) {
        headers.add(name, value);
    }
    return headers;
}
}  // namespace

TEST(CacheControlTest, ParsesTheDirectivesTheEngineActsOn) {
    const auto control = parse_cache_control("max-age=600, no-cache, must-revalidate, private");
    EXPECT_EQ(control.max_age.value_or(-1), 600);
    EXPECT_TRUE(control.no_cache);
    EXPECT_TRUE(control.must_revalidate);
    EXPECT_TRUE(control.is_private);
    EXPECT_FALSE(control.no_store);
}

// A non-numeric argument is a directive we did not understand, NOT zero.
// Reading `max-age=abc` as "expires immediately" would let one typo silently
// disable caching for a whole site, and it would look like the site's fault.
TEST(CacheControlTest, AMalformedMaxAgeIsIgnoredRatherThanReadAsZero) {
    EXPECT_FALSE(parse_cache_control("max-age=abc").max_age.has_value());
    EXPECT_FALSE(parse_cache_control("max-age=").max_age.has_value());
    // Quoted, which servers do send despite the grammar.
    EXPECT_EQ(parse_cache_control("max-age=\"300\"").max_age.value_or(-1), 300);
    // An unknown directive must not disturb the ones around it.
    const auto mixed = parse_cache_control("stale-while-revalidate=30, max-age=60");
    EXPECT_EQ(mixed.max_age.value_or(-1), 60);
}

// THE load-bearing case, in Wikipedia's own words. `s-maxage` addresses SHARED
// caches; a browser is a private one and must use `max-age`. Reading the wrong
// number here caches an article for 14 days instead of 5 minutes — and it would
// present as "Hummingbird shows stale Wikipedia pages", not as a cache bug.
TEST(CacheControlTest, SMaxAgeIsSeenAndDeliberatelyNotHonored) {
    const std::string live = "s-maxage=1209600, max-age=300, must-revalidate";
    const auto control = parse_cache_control(live);
    EXPECT_EQ(control.s_maxage.value_or(-1), 1209600) << "it must be parsed, so the choice is visible";
    EXPECT_EQ(control.max_age.value_or(-1), 300);

    const auto freshness = compute_freshness(headers_of({{"Cache-Control", live.c_str()}}), CacheTime{});
    EXPECT_EQ(freshness.lifetime.count(), 300) << "the private-cache lifetime, not the CDN's";
}

TEST(CacheControlTest, MaxAgeWinsOverExpires) {
    const auto headers = headers_of({{"Cache-Control", "max-age=60"},
                                     {"Date", "Wed, 29 Jul 2026 12:00:00 GMT"},
                                     {"Expires", "Wed, 29 Jul 2026 20:00:00 GMT"}});
    EXPECT_EQ(compute_freshness(headers, at("Wed, 29 Jul 2026 12:00:00 GMT")).lifetime.count(), 60);
}

TEST(CacheControlTest, ExpiresIsRelativeToDateWhenThereIsNoMaxAge) {
    const auto headers = headers_of(
        {{"Date", "Wed, 29 Jul 2026 12:00:00 GMT"}, {"Expires", "Wed, 29 Jul 2026 12:30:00 GMT"}});
    EXPECT_EQ(compute_freshness(headers, at("Wed, 29 Jul 2026 12:00:00 GMT")).lifetime.count(), 1800);
}

// Servers really do send `Expires: 0` and `Expires: -1` to mean "already
// expired". Neither parses as a date, and the right reading is "stale", not
// "no expiry stated" — which happens to be the same answer here, and the test
// exists so it stays that way if a heuristic is ever added.
TEST(CacheControlTest, AnUnparseableExpiresMeansStaleNotUnlimited) {
    for (const char* value : {"0", "-1", "garbage"}) {
        const auto headers = headers_of({{"Date", "Wed, 29 Jul 2026 12:00:00 GMT"}, {"Expires", value}});
        EXPECT_EQ(compute_freshness(headers, at("Wed, 29 Jul 2026 12:00:00 GMT")).lifetime.count(), 0)
            << "Expires: " << value;
    }
    // Expires already in the past is the same answer.
    const auto past = headers_of(
        {{"Date", "Wed, 29 Jul 2026 12:00:00 GMT"}, {"Expires", "Wed, 29 Jul 2026 11:00:00 GMT"}});
    EXPECT_EQ(compute_freshness(past, at("Wed, 29 Jul 2026 12:00:00 GMT")).lifetime.count(), 0);
}

// Wikipedia's actual shape: served from a CDN that has been holding it for
// hours, against a 5-minute lifetime. It is therefore STALE the moment it
// arrives. A cache that ignored `Age` would serve it for 5 minutes it was never
// granted — and would be wrong about a response it had only just received.
TEST(CacheControlTest, AgeMakesACdnResponseStaleOnArrival) {
    const auto headers =
        headers_of({{"Cache-Control", "s-maxage=1209600, max-age=300"}, {"Age", "11914"}});
    const auto freshness = compute_freshness(headers, CacheTime{});
    EXPECT_EQ(freshness.initial_age.count(), 11914);
    EXPECT_EQ(freshness.lifetime.count(), 300);
    EXPECT_GT(freshness.initial_age, freshness.lifetime) << "stale before it was ever stored";
}

// When a proxy omits `Age`, transit time inferred from `Date` has to stand in.
TEST(CacheControlTest, TransitTimeCountsAsAgeWhenAgeIsAbsent) {
    const auto headers =
        headers_of({{"Cache-Control", "max-age=600"}, {"Date", "Wed, 29 Jul 2026 12:00:00 GMT"}});
    // Received two minutes after it was generated.
    const auto freshness = compute_freshness(headers, at("Wed, 29 Jul 2026 12:02:00 GMT"));
    EXPECT_EQ(freshness.initial_age.count(), 120);
}

TEST(CacheControlTest, NoCacheMeansStorableButNeverReusedUnasked) {
    const auto freshness =
        compute_freshness(headers_of({{"Cache-Control", "no-cache, max-age=600"}}), CacheTime{});
    EXPECT_TRUE(freshness.always_revalidate);
    // The lifetime is still read; `no-cache` overrides its USE, not its value.
    EXPECT_EQ(freshness.lifetime.count(), 600);
}

// --- storability ------------------------------------------------------------

TEST(CacheControlTest, StoresAnOrdinaryCacheableResponse) {
    EXPECT_EQ(storability("GET", 200, {}, headers_of({{"Cache-Control", "max-age=3600"}})),
              Storability::Storable);
}

TEST(CacheControlTest, RefusesNoStore) {
    EXPECT_EQ(storability("GET", 200, {}, headers_of({{"Cache-Control", "no-store"}})), Storability::NoStore);
}

// `private` IS storable here, reversing what 9.3.1 shipped. The directive is
// addressed to SHARED caches — "do not keep this where another user could reach
// it" — and a per-profile in-memory cache is exactly the private cache it
// permits. What makes it safe is the credentials class in the key, not a refusal;
// refusing cost hits on the logged-in pages caching helps most and bought nothing.
TEST(CacheControlTest, PrivateIsStorableInAPerProfileCache) {
    EXPECT_EQ(storability("GET", 200, {}, headers_of({{"Cache-Control", "private, max-age=60"}})),
              Storability::Storable);
    // `no-store` still wins when a server sends both.
    EXPECT_EQ(storability("GET", 200, {}, headers_of({{"Cache-Control", "private, no-store"}})),
              Storability::NoStore);
}

// `Vary: *` is a server admitting it varies on something it will not name, so no
// cache key can be correct. The only safe reading is "do not store".
TEST(CacheControlTest, VaryStarIsNeverStored) {
    EXPECT_EQ(storability("GET", 200, {}, headers_of({{"Cache-Control", "max-age=600"}, {"Vary", "*"}})),
              Storability::VaryStar);
    // Even buried in a list, and even spelled across two headers.
    EXPECT_EQ(
        storability("GET", 200, {}, headers_of({{"Cache-Control", "max-age=600"}, {"Vary", "accept-encoding, *"}})),
        Storability::VaryStar);
    EXPECT_EQ(storability("GET", 200, {},
                          headers_of({{"Cache-Control", "max-age=600"}, {"Vary", "accept"}, {"Vary", "*"}})),
              Storability::VaryStar);
}

// HTTP/1.0's `Pragma: no-cache`, honored only when there is no Cache-Control to
// defer to — a server that sent both meant the modern one.
TEST(CacheControlTest, PragmaNoCacheOnlyAppliesWithoutCacheControl) {
    EXPECT_EQ(storability("GET", 200, {}, headers_of({{"Pragma", "no-cache"}})), Storability::NoStore);
    EXPECT_EQ(storability("GET", 200, {},
                          headers_of({{"Pragma", "no-cache"}, {"Cache-Control", "max-age=600"}})),
              Storability::Storable);
}

// Credentials are now a cache KEY rather than a refusal (9.3.2). A response to a
// credentialed request is storable; what it must never do is answer an anonymous
// one.
TEST(CacheControlTest, CredentialsAreClassifiedRatherThanRefused) {
    EXPECT_EQ(credentials_class({}), CredentialsClass::Anonymous);
    EXPECT_EQ(credentials_class(headers_of({{"Cookie", "sid=abc"}})), CredentialsClass::Credentialed);
    EXPECT_EQ(credentials_class(headers_of({{"Authorization", "Bearer x"}})), CredentialsClass::Credentialed);

    EXPECT_EQ(storability("GET", 200, headers_of({{"Cookie", "sid=abc"}}),
                          headers_of({{"Cache-Control", "max-age=600"}})),
              Storability::Storable);
}

// A response that SETS a cookie is storable too, because the cookie is stripped
// before the entry is written rather than the whole response being thrown away.
// Caching the body while dropping the session state beats caching neither.
TEST(CacheControlTest, SetCookieIsStrippedNotAReasonToRefuse) {
    EXPECT_EQ(storability("GET", 200, {}, headers_of({{"Cache-Control", "max-age=600"}, {"Set-Cookie", "sid=abc"}})),
              Storability::Storable);
    EXPECT_TRUE(Hummingbird::Core::is_uncacheable_response_header("Set-Cookie"));
    EXPECT_TRUE(Hummingbird::Core::is_uncacheable_response_header("set-cookie2"));
    EXPECT_FALSE(Hummingbird::Core::is_uncacheable_response_header("Content-Type"));
}

// HNPWA's real `Vary`, which is why this is not a hypothetical. The names are
// lowercased, sorted and deduped so the key cannot depend on the order a server
// happened to list them in.
TEST(CacheControlTest, VaryNamesAreNormalizedForTheKey) {
    using Hummingbird::Core::vary_field_names;
    const std::vector<std::string> expected{"accept-encoding", "x-fh-requested-host"};
    EXPECT_EQ(vary_field_names(headers_of({{"Vary", "x-fh-requested-host, accept-encoding"}})), expected);
    // Different order, different case, a duplicate, and split across two headers:
    // all the same key.
    EXPECT_EQ(vary_field_names(headers_of({{"Vary", "Accept-Encoding"},
                                           {"Vary", "X-FH-Requested-Host, accept-encoding"}})),
              expected);
    EXPECT_TRUE(vary_field_names({}).empty());
}

// The selecting values are the request's, trimmed but never case-folded: header
// values are case-sensitive in general, and folding would merge variants a server
// distinguishes. An absent header selects the empty string, consistently.
TEST(CacheControlTest, SelectingValuesComeFromTheRequestAndKeepTheirCase) {
    using Hummingbird::Core::selecting_header_values;
    const std::vector<std::string> names{"accept-encoding", "x-flavour"};
    const auto values =
        selecting_header_values(names, headers_of({{"Accept-Encoding", "  gzip  "}, {"X-Flavour", "Vanilla"}}));
    EXPECT_EQ(values, (std::vector<std::string>{"gzip", "Vanilla"}));
    EXPECT_EQ(selecting_header_values(names, {}), (std::vector<std::string>{"", ""}));
}

TEST(CacheControlTest, RefusesNonGetAndUncacheableStatuses) {
    EXPECT_EQ(storability("POST", 200, {}, headers_of({{"Cache-Control", "max-age=600"}})),
              Storability::MethodNotCacheable);
    // 500 is not cacheable; 206 is excluded on purpose because this engine does
    // not do ranges and would store a partial body as though it were the whole.
    EXPECT_EQ(storability("GET", 500, {}, headers_of({{"Cache-Control", "max-age=600"}})),
              Storability::StatusNotCacheable);
    EXPECT_EQ(storability("GET", 206, {}, headers_of({{"Cache-Control", "max-age=600"}})),
              Storability::StatusNotCacheable);
    // A permanent redirect IS cacheable, which is what lets a cached 301 save a
    // whole chain.
    EXPECT_EQ(storability("GET", 301, {}, headers_of({{"Cache-Control", "max-age=600"}})),
              Storability::Storable);
}

// --- validators -------------------------------------------------------------

// Wikipedia sends a WEAK ETag. Weak comparison is exactly what `If-None-Match`
// uses for revalidation, so the prefix must survive verbatim: stripping it (or
// refusing the validator) would break revalidation against one of the two M9
// proof endpoints.
TEST(CacheControlTest, WeakEtagsAreKeptVerbatim) {
    const auto validators =
        extract_validators(headers_of({{"ETag", "W/\"a1b2c3\""}, {"Last-Modified", "Wed, 29 Jul 2026 12:00:00 GMT"}}));
    EXPECT_EQ(validators.etag, "W/\"a1b2c3\"");
    EXPECT_EQ(validators.last_modified, "Wed, 29 Jul 2026 12:00:00 GMT");
    EXPECT_TRUE(validators.any());
    EXPECT_FALSE(extract_validators({}).any());
}
