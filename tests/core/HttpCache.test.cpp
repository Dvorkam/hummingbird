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

CacheTime epoch() { return CacheTime{}; }
CacheTime plus(long seconds) { return CacheTime{} + std::chrono::seconds(seconds); }

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
    ASSERT_EQ(cache.store("GET", "https://a.test/x", 200, {}, headers_of({{"Cache-Control", "max-age=60"}}),
                          "BODY", epoch()),
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
    cache.store("GET", "https://a.test/x", 200, {},
                headers_of({{"Cache-Control", "max-age=60"}, {"ETag", "\"v1\""}}), "BODY", epoch());

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
    ASSERT_EQ(cache.store("GET", "https://a.test/x", 200, {},
                          headers_of({{"Cache-Control", "no-cache, max-age=600"}, {"ETag", "\"v1\""}}), "BODY",
                          epoch()),
              Storability::Storable);
    EXPECT_EQ(cache.lookup("GET", "https://a.test/x", {}, epoch()).outcome, Outcome::MustRevalidate);
}

TEST(HttpCacheTest, NoStoreIsNeverStored) {
    HttpCache cache;
    EXPECT_EQ(cache.store("GET", "https://a.test/x", 200, {}, headers_of({{"Cache-Control", "no-store"}}),
                          "BODY", epoch()),
              Storability::NoStore);
    EXPECT_EQ(cache.lookup("GET", "https://a.test/x", {}, epoch()).outcome, Outcome::Miss);
    EXPECT_EQ(cache.stats().entries, 0u);
}

// A 304 carries no body. Reviving the stored one is the entire point of
// revalidation: the round trip still happened, but the bytes did not.
TEST(HttpCacheTest, A304RevivesTheStoredBodyAndExtendsFreshness) {
    HttpCache cache;
    cache.store("GET", "https://a.test/x", 200, {},
                headers_of({{"Cache-Control", "max-age=60"}, {"ETag", "\"v1\""}}), "ORIGINAL", epoch());
    ASSERT_EQ(cache.lookup("GET", "https://a.test/x", {}, plus(120)).outcome, Outcome::MustRevalidate);

    // The server says "still good, and good for another minute".
    const auto revived = cache.refresh_from_not_modified(
        "GET", "https://a.test/x", {}, headers_of({{"Cache-Control", "max-age=60"}}), plus(120));
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
                headers_of({{"Cache-Control", "max-age=1"}, {"ETag", "\"v1\""}, {"Content-Length", "8"}}),
                "ORIGINAL", epoch());

    const auto revived = cache.refresh_from_not_modified(
        "GET", "https://a.test/x", {},
        headers_of({{"Cache-Control", "max-age=60"}, {"Content-Length", "0"}}), plus(10));
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
    EXPECT_EQ(cache.store("GET", "https://a.test/x", 200, {}, headers_of({{"Content-Type", "text/html"}}), "BODY",
                          epoch()),
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
    EXPECT_EQ(cache.store("GET", "https://a.test/big", 200, {}, headers_of({{"Cache-Control", "max-age=60"}}),
                          huge, epoch()),
              Storability::TooLarge);
    EXPECT_EQ(cache.stats().entries, 0u);
}

TEST(HttpCacheTest, KeysOnMethodAndUrlTogether) {
    HttpCache cache;
    const auto response = headers_of({{"Cache-Control", "max-age=60"}});
    cache.store("GET", "https://a.test/x", 200, {}, response, "BODY", epoch());
    EXPECT_EQ(cache.lookup("GET", "https://a.test/y", {}, epoch()).outcome, Outcome::Miss);
    // Query strings are part of the URL, so they are part of the key.
    EXPECT_EQ(cache.lookup("GET", "https://a.test/x?v=2", {}, epoch()).outcome, Outcome::Miss);
}
