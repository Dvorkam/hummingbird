#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <string_view>

#include "core/net/HttpHeaders.h"

namespace Hummingbird::Core {

// HTTP cache policy (RFC 9111), story 9.3.1.
//
// Deliberately a pure-function module like Cors: every decision here is a
// function of a request, a response's headers and a clock reading, so the whole
// matrix is testable without a server or a store.
//
// Wall clock, not steady: freshness is computed against `Date`, `Expires` and
// `Age`, which are absolute times a server sent. Same reasoning as cookies, and
// the same trade-off — a user who moves their clock confuses the cache. The
// alternative (steady_clock) cannot compare against a server's date at all.
using CacheClock = std::chrono::system_clock;
using CacheTime = CacheClock::time_point;

// The `Cache-Control` directives this engine acts on. Unknown directives are
// ignored, which is what the grammar requires.
struct CacheControl {
    bool no_store = false;
    // May be stored, but never reused without asking the server first.
    bool no_cache = false;
    bool must_revalidate = false;
    bool is_private = false;
    bool is_public = false;
    bool immutable = false;
    std::optional<long> max_age;

    // Parsed but deliberately NOT honored. `s-maxage` addresses SHARED caches
    // (a CDN, a corporate proxy); a browser is a private cache and must use
    // `max-age`. This is not a nicety: Wikipedia answers with
    // `s-maxage=1209600, max-age=300`, so reading the wrong one of those two
    // caches an article for 14 days instead of 5 minutes. Kept in the struct so
    // a test can pin that we saw it and ignored it.
    std::optional<long> s_maxage;
};

// Parses one `Cache-Control` field value. Tolerant by design: a malformed
// directive is skipped rather than failing the whole header, because a cache
// that discards a header it half-understood is more wrong than one that ignores
// the part it does not.
CacheControl parse_cache_control(std::string_view value);

// Why a response may not be stored. Specific rather than a bool because "not
// cached" is otherwise impossible to debug from the outside, and three of these
// are deliberate M9 conservatism that 9.3.2 revisits.
enum class Storability {
    Storable,
    // Only GET is cached in M9. HEAD shares a URL with GET and would need care
    // to avoid answering a GET from a bodyless entry; POST is not cacheable
    // without the request body in the key.
    MethodNotCacheable,
    StatusNotCacheable,
    NoStore,
    // `Cache-Control: private`. Over-strict for a browser — `private` is aimed
    // at shared caches, and a per-profile memory cache IS the privacy boundary
    // it protects — but M9 refuses it, because the credentialed-response rule
    // that would make storing it safe is 9.3.2's job. See the story notes.
    Private,
    // --- deliberate M9 conservatism; 9.3.2 turns each into a cache KEY --------
    // A response that carries `Set-Cookie` is per-request state by definition,
    // and replaying it from cache would resurrect a cookie the user deleted.
    HasSetCookie,
    // The request carried `Cookie` or `Authorization`. Storing the answer would
    // risk serving one user's session to an anonymous request. 9.3.2 decides
    // and documents the credentialed rule; until then, refuse.
    RequestHadCredentials,
    // Any `Vary` at all. A Vary-blind cache serves the wrong variant, and it
    // presents as an unreproducible rendering bug rather than as a cache bug —
    // so until 9.3.2 keys on it, these are not stored. This is what makes the
    // 9.3.1 cache conservative-but-correct rather than fast-and-wrong.
    HasVary,

    // --- decided by the store rather than by policy --------------------------
    // Stale on arrival AND carrying no validator: it would fail every freshness
    // check and offer nothing to revalidate with, so storing it is pure cost.
    NothingToReuse,
    // Over the per-entry cap, so that one large download cannot flush the cache.
    TooLarge,
};

std::string_view describe(Storability storability);

// Whether a response to `method` may be stored at all. `request_headers` are the
// headers actually sent (after cookies and identity were applied), not the ones
// the caller asked for.
Storability storability(std::string_view method, long status, const HttpHeaders& request_headers,
                        const HttpHeaders& response_headers);

// How long a stored response stays usable, and how old it already was.
struct Freshness {
    // How long after generation the response is fresh. Zero means it arrived
    // stale: still worth storing, because revalidation saves the BODY even when
    // it cannot save the round trip.
    std::chrono::seconds lifetime{0};

    // Age at the moment it reached us: the `Age` header, or transit time
    // inferred from `Date`, whichever is larger (RFC 9111 §4.2.3). This is not a
    // corner case — Wikipedia serves from a CDN with `Age: 11914` against
    // `max-age=300`, so its responses are stale on arrival and every reuse is a
    // revalidation. A cache that ignored `Age` would serve those for 5 minutes
    // it was never granted.
    std::chrono::seconds initial_age{0};

    // `no-cache`: storable, but every reuse must be revalidated first.
    bool always_revalidate = false;
};

// `received_at` is when the response arrived, which is also what `Date` is
// compared against to infer transit time.
//
// NOTE: no heuristic freshness. RFC 9111 permits guessing a lifetime from
// `Last-Modified` when the server states none; this engine does not, so a
// response with only validators is stale immediately and gets revalidated on
// every use. Conservative on purpose — a guessed lifetime shows up as a page
// that will not update, and the revalidation still saves the body transfer.
Freshness compute_freshness(const HttpHeaders& response_headers, CacheTime received_at);

// What a conditional request can be built from.
struct Validators {
    // Verbatim as sent, `W/` prefix included. A weak validator is perfectly
    // usable for revalidation — weak comparison is what `If-None-Match` uses —
    // and only byte-range reuse needs a strong one. Wikipedia sends weak ETags,
    // so stripping the prefix here would break revalidation against it.
    std::string etag;
    std::string last_modified;

    bool any() const { return !etag.empty() || !last_modified.empty(); }
};

Validators extract_validators(const HttpHeaders& response_headers);

}  // namespace Hummingbird::Core
