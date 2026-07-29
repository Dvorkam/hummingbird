#pragma once

#include <chrono>
#include <cstddef>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/net/CacheControl.h"
#include "core/net/HttpHeaders.h"

namespace Hummingbird::Core {

// The in-memory HTTP cache (story 9.3.1).
//
// Memory only, dropped at exit — an on-disk cache is an explicit M9 non-goal and
// pairs with the profile-data work. Shared per profile like the cookie jar, so a
// second tab visiting the same page benefits from the first one's fetches.
//
// SELF-SYNCHRONIZING, unlike CookieJar, which its caller locks externally. The
// distinction is real rather than stylistic: every method here is one complete
// operation that returns a copy, whereas `CookieJar::cookies_for` hands back a
// vector the caller then works with, so its lock has to outlive the call. Owning
// the lock here matters because network callbacks arrive on backend threads
// while the request path is running on another.
class HttpCache {
public:
    // Bounds, chosen so a single large download cannot evict everything else.
    // Modest on purpose: this cache exists to stop a page refetching the same
    // JSON six times, not to be a browser-grade disk cache.
    static constexpr size_t kMaxBytes = 8u * 1024 * 1024;
    static constexpr size_t kMaxEntryBytes = 2u * 1024 * 1024;
    static constexpr size_t kMaxEntries = 300;

    enum class Outcome {
        // Nothing stored for this request.
        Miss,
        // Stored and still fresh: serve it without touching the network.
        Fresh,
        // Stored but stale (or `no-cache`): the entry is still valuable, but the
        // server must confirm it first. This is the common case against real
        // CDNs, and it is not a failure — a 304 saves the body, which is almost
        // all of the bytes.
        MustRevalidate,
    };

    struct Lookup {
        Outcome outcome = Outcome::Miss;

        // Populated for Fresh: the response to serve. Headers are as the server
        // sent them, NOT filtered for CORS — policy re-applies on every use, so
        // what is stored has to be the raw answer.
        long status = 0;
        HttpHeaders headers;
        std::string body;

        // Populated whenever an entry exists at all, including a fresh one: a
        // forced revalidation (F5) needs them for an entry it would otherwise
        // have served outright.
        std::string etag;
        std::string last_modified;

        // How old the stored response is now. Written into the served response's
        // `Age` header, which is both spec-correct and the one standard way a
        // page can see that an answer came from cache.
        std::chrono::seconds age{0};
    };

    // `request_headers` are the headers actually being sent, after cookies and
    // identity were applied. 9.3.1 does not key on them; 9.3.2 will, and taking
    // them from the start is what makes that a fill-in rather than a retrofit.
    Lookup lookup(std::string_view method, std::string_view url, const HttpHeaders& request_headers, CacheTime now);

    // Stores the response, or does nothing and reports why. Returns the reason
    // rather than a bool because "why was this not cached" is otherwise
    // unanswerable from outside, and three of the reasons are M9 conservatism
    // rather than the server's doing.
    Storability store(std::string_view method, std::string_view url, long status, const HttpHeaders& request_headers,
                      const HttpHeaders& response_headers, std::string body, CacheTime now);

    // A conditional request came back 304. Refreshes the stored entry's headers
    // and freshness, then hands back the full response to serve in its place.
    //
    // Returns nullopt when there is no longer an entry to revive — possible when
    // the entry was evicted between issuing the conditional request and its
    // answer arriving. The caller must then re-request unconditionally; a 304
    // carries no body, so there is nothing else to hand the page.
    std::optional<Lookup> refresh_from_not_modified(std::string_view method, std::string_view url,
                                                    const HttpHeaders& request_headers,
                                                    const HttpHeaders& not_modified_headers, CacheTime now);

    // Counters, for tests and for the demo page. A cache is invisible by nature,
    // which is exactly what makes it hard to trust; these are how it is shown to
    // be working rather than asserted to be.
    struct Stats {
        size_t hits = 0;           // served with no network request at all
        size_t revalidations = 0;  // conditional requests issued
        size_t not_modified = 0;   // 304s that revived a stored body
        size_t misses = 0;
        size_t stores = 0;
        size_t evictions = 0;
        size_t bytes = 0;
        size_t entries = 0;
    };
    Stats stats() const;

    void clear();

private:
    struct Entry {
        std::string method;
        std::string url;
        long status = 0;
        HttpHeaders headers;
        std::string body;
        CacheTime received_at{};
        CacheTime last_access{};
        std::chrono::seconds initial_age{0};
        std::chrono::seconds lifetime{0};
        bool always_revalidate = false;
        std::string etag;
        std::string last_modified;

        size_t footprint() const;
        std::chrono::seconds current_age(CacheTime now) const;
        bool is_fresh(CacheTime now) const;
    };

    // Called with `mutex_` held.
    Entry* find(std::string_view method, std::string_view url);
    // Drops least-recently-used entries until `incoming` would fit both caps.
    void evict_for(size_t incoming);

    // A flat vector rather than a map plus an LRU list: at kMaxEntries the scan
    // is a few hundred string compares per request, which is nothing next to the
    // network call it might save, and it keeps eviction to one obvious loop.
    std::vector<Entry> entries_;
    size_t bytes_ = 0;
    Stats stats_;
    mutable std::mutex mutex_;
};

}  // namespace Hummingbird::Core
