// Story 9.3.1: the cache store itself — freshness over time, revalidation, and
// the bounds that keep it from becoming a memory leak with a lookup function.
//
// Every test drives the clock explicitly. A cache whose tests depend on real
// time is a cache whose failures cannot be reproduced.
#include "core/net/HttpCache.h"

#include <gtest/gtest.h>

#include <chrono>
#include <initializer_list>
#include <string>
#include <utility>

namespace {
using Hummingbird::Core::CacheTime;
using Hummingbird::Core::HttpCache;
using Hummingbird::Core::HttpHeaders;
using Hummingbird::Core::Storability;
using Outcome = HttpCache::Outcome;

CacheTime epoch() {
    return CacheTime{};
}
CacheTime plus(long seconds) {
    return CacheTime{} + std::chrono::seconds(seconds);
}

HttpHeaders headers_of(std::initializer_list<std::pair<const char*, const char*>> fields) {
    HttpHeaders headers;
    for (const auto& [name, value] : fields) {
        headers.add(name, value);
    }
    return headers;
}
}  // namespace

TEST(HttpCacheTest, ServesAFreshEntryWithoutAskingTheNetwork) {
    HttpCache cache;
    ASSERT_EQ(
        cache.store("GET", "https://a.test/x", 200, {}, headers_of({{"Cache-Control", "max-age=60"}}), "BODY", epoch()),
        Storability::Storable);

    const auto hit = cache.lookup("GET", "https://a.test/x", {}, plus(30));
    EXPECT_EQ(hit.outcome, Outcome::Fresh);
    EXPECT_EQ(hit.status, 200);
    EXPECT_EQ(hit.body, "BODY");
    EXPECT_EQ(hit.age.count(), 30);
    EXPECT_EQ(cache.stats().hits, 1u);
}

// The transition that matters: the same entry, the same cache, one second past
// its lifetime. It does not vanish — it becomes something to confirm.
TEST(HttpCacheTest, AnExpiredEntryBecomesARevalidationRatherThanAMiss) {
    HttpCache cache;
    cache.store("GET", "https://a.test/x", 200, {}, headers_of({{"Cache-Control", "max-age=60"}, {"ETag", "\"v1\""}}),
                "BODY", epoch());

    EXPECT_EQ(cache.lookup("GET", "https://a.test/x", {}, plus(59)).outcome, Outcome::Fresh);

    const auto stale = cache.lookup("GET", "https://a.test/x", {}, plus(61));
    EXPECT_EQ(stale.outcome, Outcome::MustRevalidate);
    EXPECT_EQ(stale.etag, "\"v1\"") << "the caller needs this to build If-None-Match";
    EXPECT_TRUE(stale.body.empty()) << "a stale entry must not hand back a body to serve";
}

// `no-cache` is not `no-store`: the body is kept, but it may never be served
// without asking first. Confusing the two costs either correctness or every
// revalidation's savings.
TEST(HttpCacheTest, NoCacheIsStoredButNeverServedFresh) {
    HttpCache cache;
    ASSERT_EQ(
        cache.store("GET", "https://a.test/x", 200, {},
                    headers_of({{"Cache-Control", "no-cache, max-age=600"}, {"ETag", "\"v1\""}}), "BODY", epoch()),
        Storability::Storable);
    EXPECT_EQ(cache.lookup("GET", "https://a.test/x", {}, epoch()).outcome, Outcome::MustRevalidate);
}

TEST(HttpCacheTest, NoStoreIsNeverStored) {
    HttpCache cache;
    EXPECT_EQ(
        cache.store("GET", "https://a.test/x", 200, {}, headers_of({{"Cache-Control", "no-store"}}), "BODY", epoch()),
        Storability::NoStore);
    EXPECT_EQ(cache.lookup("GET", "https://a.test/x", {}, epoch()).outcome, Outcome::Miss);
    EXPECT_EQ(cache.stats().entries, 0u);
}

// A 304 carries no body. Reviving the stored one is the entire point of
// revalidation: the round trip still happened, but the bytes did not.
TEST(HttpCacheTest, A304RevivesTheStoredBodyAndExtendsFreshness) {
    HttpCache cache;
    cache.store("GET", "https://a.test/x", 200, {}, headers_of({{"Cache-Control", "max-age=60"}, {"ETag", "\"v1\""}}),
                "ORIGINAL", epoch());
    ASSERT_EQ(cache.lookup("GET", "https://a.test/x", {}, plus(120)).outcome, Outcome::MustRevalidate);

    // The server says "still good, and good for another minute".
    const auto revived = cache.refresh_from_not_modified("GET", "https://a.test/x", {},
                                                         headers_of({{"Cache-Control", "max-age=60"}}), plus(120));
    ASSERT_TRUE(revived.has_value());
    EXPECT_EQ(revived->status, 200);
    EXPECT_EQ(revived->body, "ORIGINAL");

    // And the extension took effect: it is fresh again, from the 304's moment.
    EXPECT_EQ(cache.lookup("GET", "https://a.test/x", {}, plus(150)).outcome, Outcome::Fresh);
    EXPECT_EQ(cache.stats().not_modified, 1u);
}

// A 304 says "your copy is current" — it does not describe its own emptiness as
// the resource's. Copying its `Content-Length: 0` onto the revived response
// would claim the body about to be served is empty.
TEST(HttpCacheTest, A304DoesNotOverwriteTheStoredBodyHeaders) {
    HttpCache cache;
    cache.store("GET", "https://a.test/x", 200, {},
                headers_of({{"Cache-Control", "max-age=1"}, {"ETag", "\"v1\""}, {"Content-Length", "8"}}), "ORIGINAL",
                epoch());

    const auto revived = cache.refresh_from_not_modified(
        "GET", "https://a.test/x", {}, headers_of({{"Cache-Control", "max-age=60"}, {"Content-Length", "0"}}),
        plus(10));
    ASSERT_TRUE(revived.has_value());
    EXPECT_EQ(revived->body, "ORIGINAL");
    EXPECT_EQ(revived->headers.get("Content-Length"), "8") << "the 304's own length is not the resource's";
    // Non-body headers DO update, which is how a server extends a lifetime.
    EXPECT_EQ(revived->headers.get("Cache-Control"), "max-age=60");
}

// The race the caller has to handle: the entry was evicted between issuing the
// conditional request and its answer arriving. There is nothing to revive, and
// a 304 has no body, so the store must say so rather than invent one.
TEST(HttpCacheTest, A304WithNothingStoredReportsFailureRatherThanAnEmptyBody) {
    HttpCache cache;
    EXPECT_FALSE(
        cache.refresh_from_not_modified("GET", "https://a.test/gone", {}, headers_of({{"ETag", "\"v1\""}}), epoch())
            .has_value());
}

// Stale AND unvalidatable is the one genuinely worthless combination: it fails
// every freshness check and offers nothing to revalidate with, so keeping it
// would be memory spent on something that can never be used.
TEST(HttpCacheTest, DoesNotStoreWhatCouldNeverBeReused) {
    HttpCache cache;
    EXPECT_EQ(
        cache.store("GET", "https://a.test/x", 200, {}, headers_of({{"Content-Type", "text/html"}}), "BODY", epoch()),
        Storability::NothingToReuse);
    EXPECT_EQ(cache.stats().entries, 0u);

    // With a validator and no lifetime it IS worth keeping: stale on arrival,
    // but a 304 will still save the body. This is Wikipedia's shape.
    EXPECT_EQ(cache.store("GET", "https://a.test/y", 200, {},
                          headers_of({{"Cache-Control", "max-age=300"}, {"Age", "11914"}, {"ETag", "W/\"v1\""}}),
                          "BODY", epoch()),
              Storability::Storable);
    EXPECT_EQ(cache.lookup("GET", "https://a.test/y", {}, epoch()).outcome, Outcome::MustRevalidate)
        << "Age exceeds max-age, so it is stale the moment it is stored";
}

TEST(HttpCacheTest, ReplacingAnEntryDoesNotDoubleCountItsBytes) {
    HttpCache cache;
    const auto response = headers_of({{"Cache-Control", "max-age=60"}});
    cache.store("GET", "https://a.test/x", 200, {}, response, "FIRST", epoch());
    const size_t after_first = cache.stats().bytes;
    cache.store("GET", "https://a.test/x", 200, {}, response, "SECOND", plus(1));

    EXPECT_EQ(cache.stats().entries, 1u);
    EXPECT_EQ(cache.stats().bytes, after_first + 1) << "\"SECOND\" is one byte longer than \"FIRST\"";
    EXPECT_EQ(cache.lookup("GET", "https://a.test/x", {}, plus(2)).body, "SECOND");
}

// The bound has to hold, and it has to sacrifice the right entry: the one
// nobody has looked at, not the one that happens to be first in the vector.
TEST(HttpCacheTest, EvictsTheLeastRecentlyUsedEntryWhenFull) {
    HttpCache cache;
    const auto response = headers_of({{"Cache-Control", "max-age=100000"}});
    for (size_t i = 0; i < HttpCache::kMaxEntries; ++i) {
        cache.store("GET", "https://a.test/" + std::to_string(i), 200, {}, response, "BODY",
                    plus(static_cast<long>(i)));
    }
    ASSERT_EQ(cache.stats().entries, HttpCache::kMaxEntries);

    // Touch the oldest so it is no longer the least recently used.
    ASSERT_EQ(cache.lookup("GET", "https://a.test/0", {}, plus(1000)).outcome, Outcome::Fresh);
    cache.store("GET", "https://a.test/new", 200, {}, response, "BODY", plus(2000));

    EXPECT_EQ(cache.stats().entries, HttpCache::kMaxEntries);
    EXPECT_EQ(cache.stats().evictions, 1u);
    EXPECT_EQ(cache.lookup("GET", "https://a.test/0", {}, plus(2000)).outcome, Outcome::Fresh)
        << "the recently used entry survived";
    EXPECT_EQ(cache.lookup("GET", "https://a.test/1", {}, plus(2000)).outcome, Outcome::Miss)
        << "the least recently used entry was the victim";
}

// One large download must not be able to flush everything else out.
TEST(HttpCacheTest, RefusesAnEntryLargerThanThePerEntryCap) {
    HttpCache cache;
    const std::string huge(HttpCache::kMaxEntryBytes + 1, 'x');
    EXPECT_EQ(
        cache.store("GET", "https://a.test/big", 200, {}, headers_of({{"Cache-Control", "max-age=60"}}), huge, epoch()),
        Storability::TooLarge);
    EXPECT_EQ(cache.stats().entries, 0u);
}

TEST(HttpCacheTest, VarySelectingValuesCountTowardThePerEntryCap) {
    HttpCache cache;
    const std::string huge(HttpCache::kMaxEntryBytes, 'x');
    const auto request = headers_of({{"X-Selector", huge.c_str()}});
    const auto response = headers_of({{"Cache-Control", "max-age=60"}, {"Vary", "X-Selector"}});

    EXPECT_EQ(cache.store("GET", "https://a.test/vary", 200, request, response, "BODY", epoch()),
              Storability::TooLarge);
    EXPECT_EQ(cache.stats().entries, 0u);
    EXPECT_EQ(cache.stats().bytes, 0u);
}

TEST(HttpCacheTest, VarySelectingValuesParticipateInTotalCapEviction) {
    HttpCache cache;
    const auto response = headers_of({{"Cache-Control", "max-age=60"}, {"Vary", "X-Selector"}});
    const std::string large_selector(1900u * 1024u, 'x');

    for (size_t i = 0; i < 5; ++i) {
        auto selected = large_selector;
        selected.back() = static_cast<char>('a' + i);
        const auto request = headers_of({{"X-Selector", selected.c_str()}});
        EXPECT_EQ(cache.store("GET", "https://a.test/vary/" + std::to_string(i), 200, request, response, "BODY",
                              plus(static_cast<long>(i))),
                  Storability::Storable);
    }

    EXPECT_LE(cache.stats().bytes, HttpCache::kMaxBytes);
    EXPECT_LT(cache.stats().entries, 5u);
    EXPECT_GT(cache.stats().evictions, 0u);
}

// --- the secondary key: Vary and credentials (story 9.3.2) -------------------

// The acceptance criterion, stated exactly: two requests differing only in a
// Vary-named header do not share an entry. A cache that got this wrong would
// serve the wrong variant, and it would look like a rendering bug.
TEST(HttpCacheTest, TwoRequestsDifferingInAVaryNamedHeaderDoNotShareAnEntry) {
    HttpCache cache;
    const auto response = headers_of({{"Cache-Control", "max-age=60"}, {"Vary", "X-Flavour"}});

    const auto vanilla = headers_of({{"X-Flavour", "vanilla"}});
    const auto chocolate = headers_of({{"X-Flavour", "chocolate"}});
    cache.store("GET", "https://a.test/x", 200, vanilla, response, "VANILLA", epoch());

    EXPECT_EQ(cache.lookup("GET", "https://a.test/x", vanilla, epoch()).body, "VANILLA");
    EXPECT_EQ(cache.lookup("GET", "https://a.test/x", chocolate, epoch()).outcome, Outcome::Miss)
        << "the other variant was never stored, so this must not be a hit";

    // Both variants coexist under one URL, each answering only its own request.
    cache.store("GET", "https://a.test/x", 200, chocolate, response, "CHOCOLATE", epoch());
    EXPECT_EQ(cache.lookup("GET", "https://a.test/x", vanilla, epoch()).body, "VANILLA");
    EXPECT_EQ(cache.lookup("GET", "https://a.test/x", chocolate, epoch()).body, "CHOCOLATE");
    EXPECT_EQ(cache.stats().entries, 2u);
}

// A header the response does NOT vary on is not part of the key, so changing it
// must still hit. Getting this backwards would make the cache useless — every
// request differs in something.
TEST(HttpCacheTest, HeadersTheResponseDoesNotVaryOnAreNotPartOfTheKey) {
    HttpCache cache;
    cache.store("GET", "https://a.test/x", 200, headers_of({{"X-Flavour", "vanilla"}}),
                headers_of({{"Cache-Control", "max-age=60"}, {"Vary", "Accept-Language"}}), "BODY", epoch());
    EXPECT_EQ(cache.lookup("GET", "https://a.test/x", headers_of({{"X-Flavour", "chocolate"}}), epoch()).outcome,
              Outcome::Fresh);
}

// The credentialed rule story 9.3.2 was asked to decide. The cache is per
// profile, so this is not about separating users — it is about not letting an
// anonymous request read a personalized answer that happens to be in memory.
TEST(HttpCacheTest, ACredentialedResponseIsNeverServedToAnAnonymousRequest) {
    HttpCache cache;
    const auto response = headers_of({{"Cache-Control", "private, max-age=60"}});
    const auto with_cookie = headers_of({{"Cookie", "sid=abc"}});

    ASSERT_EQ(cache.store("GET", "https://a.test/me", 200, with_cookie, response, "YOUR PROFILE", epoch()),
              Storability::Storable)
        << "private IS storable in a per-profile cache; the credentials class is what protects it";

    EXPECT_EQ(cache.lookup("GET", "https://a.test/me", with_cookie, epoch()).body, "YOUR PROFILE");
    EXPECT_EQ(cache.lookup("GET", "https://a.test/me", {}, epoch()).outcome, Outcome::Miss)
        << "an anonymous request must not receive the credentialed response";
    // And the reverse: an anonymous response must not answer a credentialed one.
    cache.store("GET", "https://a.test/pub", 200, {}, headers_of({{"Cache-Control", "max-age=60"}}), "GENERIC",
                epoch());
    EXPECT_EQ(cache.lookup("GET", "https://a.test/pub", with_cookie, epoch()).outcome, Outcome::Miss);
}

// A cached copy must never carry the session token that came with it. Stripping
// Set-Cookie is what makes such a response cacheable at all.
TEST(HttpCacheTest, SetCookieIsNeverWrittenIntoAnEntry) {
    HttpCache cache;
    ASSERT_EQ(cache.store("GET", "https://a.test/login", 200, {},
                          headers_of({{"Cache-Control", "max-age=60"}, {"Set-Cookie", "sid=secret"}}), "PAGE", epoch()),
              Storability::Storable);
    const auto hit = cache.lookup("GET", "https://a.test/login", {}, epoch());
    ASSERT_EQ(hit.outcome, Outcome::Fresh);
    EXPECT_EQ(hit.body, "PAGE");
    EXPECT_TRUE(hit.headers.get("Set-Cookie").empty()) << "a session token must not survive in the cache";
}

// `Vary` lets one URL occupy unbounded entries. A page churning a header must
// pay for that itself rather than flushing everything else out.
TEST(HttpCacheTest, VariantsOfOneUrlAreCappedAndEvictLocally) {
    HttpCache cache;
    const auto response = headers_of({{"Cache-Control", "max-age=100000"}, {"Vary", "X-N"}});
    cache.store("GET", "https://a.test/other", 200, {}, headers_of({{"Cache-Control", "max-age=100000"}}), "KEEP",
                epoch());

    for (size_t i = 0; i < HttpCache::kMaxVariantsPerUrl + 4; ++i) {
        cache.store("GET", "https://a.test/x", 200, headers_of({{"X-N", std::to_string(i).c_str()}}), response, "BODY",
                    plus(static_cast<long>(i)));
    }

    EXPECT_LE(cache.stats().entries, HttpCache::kMaxVariantsPerUrl + 1);
    EXPECT_EQ(cache.lookup("GET", "https://a.test/other", {}, plus(1000)).outcome, Outcome::Fresh)
        << "an unrelated URL must survive one URL's variant churn";
    // The newest variant is present; the oldest was the victim.
    EXPECT_EQ(cache
                  .lookup("GET", "https://a.test/x",
                          headers_of({{"X-N", std::to_string(HttpCache::kMaxVariantsPerUrl + 3).c_str()}}), plus(1000))
                  .outcome,
              Outcome::Fresh);
    EXPECT_EQ(cache.lookup("GET", "https://a.test/x", headers_of({{"X-N", "0"}}), plus(1000)).outcome, Outcome::Miss);
}

// A 304 confirms the variant the conditional request was built from, not merely
// "the entry for this URL".
TEST(HttpCacheTest, RevalidationRefreshesOnlyTheMatchingVariant) {
    HttpCache cache;
    const auto response = headers_of({{"Cache-Control", "max-age=60"}, {"Vary", "X-Flavour"}, {"ETag", "\"v1\""}});
    const auto vanilla = headers_of({{"X-Flavour", "vanilla"}});
    const auto chocolate = headers_of({{"X-Flavour", "chocolate"}});
    cache.store("GET", "https://a.test/x", 200, vanilla, response, "VANILLA", epoch());
    cache.store("GET", "https://a.test/x", 200, chocolate, response, "CHOCOLATE", epoch());

    const auto revived = cache.refresh_from_not_modified("GET", "https://a.test/x", chocolate,
                                                         headers_of({{"Cache-Control", "max-age=60"}}), plus(120));
    ASSERT_TRUE(revived.has_value());
    EXPECT_EQ(revived->body, "CHOCOLATE") << "the 304 revived the variant that was asked about";
    // The vanilla variant was left stale, because nothing confirmed it.
    EXPECT_EQ(cache.lookup("GET", "https://a.test/x", vanilla, plus(150)).outcome, Outcome::MustRevalidate);
    EXPECT_EQ(cache.lookup("GET", "https://a.test/x", chocolate, plus(150)).outcome, Outcome::Fresh);
}

TEST(HttpCacheTest, RevalidationNeverStoresASetCookieHeader) {
    HttpCache cache;
    cache.store("GET", "https://a.test/x", 200, {}, headers_of({{"Cache-Control", "max-age=0"}, {"ETag", "\"v1\""}}),
                "BODY", epoch());

    const auto revived = cache.refresh_from_not_modified(
        "GET", "https://a.test/x", {}, headers_of({{"Cache-Control", "max-age=60"}, {"Set-Cookie", "session=secret"}}),
        plus(1));

    ASSERT_TRUE(revived.has_value());
    EXPECT_TRUE(revived->headers.get("Set-Cookie").empty()) << "a 304 must not smuggle a cookie into the cache";
    const auto hit = cache.lookup("GET", "https://a.test/x", {}, plus(2));
    ASSERT_EQ(hit.outcome, Outcome::Fresh);
    EXPECT_TRUE(hit.headers.get("Set-Cookie").empty());
}

TEST(HttpCacheTest, KeysOnMethodAndUrlTogether) {
    HttpCache cache;
    const auto response = headers_of({{"Cache-Control", "max-age=60"}});
    cache.store("GET", "https://a.test/x", 200, {}, response, "BODY", epoch());
    EXPECT_EQ(cache.lookup("GET", "https://a.test/y", {}, epoch()).outcome, Outcome::Miss);
    // Query strings are part of the URL, so they are part of the key.
    EXPECT_EQ(cache.lookup("GET", "https://a.test/x?v=2", {}, epoch()).outcome, Outcome::Miss);
}
