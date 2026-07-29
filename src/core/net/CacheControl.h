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

// Whether a request carried the user's ambient credentials. Part of the cache
// key (story 9.3.2): a response fetched with a session must never be served to a
// request that had none, nor the other way round.
//
// Note what this deliberately does NOT include: the cookie's VALUE. Keying on
// that would invalidate the whole cache every time any cookie changed, and it is
// not what browsers do — a server whose response genuinely depends on which user
// is asking is expected to say `Vary: Cookie`, which this cache then honors. The
// division of responsibility is the server's to declare and ours to respect.
enum class CredentialsClass {
    Anonymous,
    Credentialed,
};

CredentialsClass credentials_class(const HttpHeaders& request_headers);

// Why a response may not be stored. Specific rather than a bool because "not
// cached" is otherwise impossible to debug from the outside.
enum class Storability {
    Storable,
    // Only GET is cached in M9. HEAD shares a URL with GET and would need care
    // to avoid answering a GET from a bodyless entry; POST is not cacheable
    // without the request body in the key.
    MethodNotCacheable,
    StatusNotCacheable,
    NoStore,
    // `Vary: *` means "this varies on something I am not going to name", which is
    // an admission that no cache key can be correct. The only safe reading is
    // "do not store" (RFC 9111 §4.1).
    VaryStar,

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

// --- the secondary cache key: `Vary` (story 9.3.2) ---------------------------
//
// A cache keyed only on method + URL is wrong the moment a response depends on a
// request header, and it fails in the worst possible way: it serves the wrong
// variant, which presents as an unreproducible RENDERING bug rather than as a
// cache bug. M8 made this concrete by giving each origin its own `User-Agent`,
// and HNPWA — one of M9's two proof endpoints — really does answer with
// `Vary: x-fh-requested-host, accept-encoding`.

// The field names a response's `Vary` names: lowercased, sorted and deduped so
// the key does not depend on the order the server listed them in. Empty when
// there is no `Vary`; `*` is reported separately by `vary_is_star`.
std::vector<std::string> vary_field_names(const HttpHeaders& response_headers);

bool vary_is_star(const HttpHeaders& response_headers);

// The request's values for `names`, in the same order — the "selecting header
// values" of RFC 9111 §4.1. Two requests match only if these agree, which is
// what makes one URL able to hold several variants without confusing them.
//
// LIMITATION worth knowing: these come from the headers the ENGINE set, and the
// transport owns a few of its own (notably `Accept-Encoding`, which libcurl
// sets). A response varying on one of those is keyed on our empty value. That is
// consistent rather than wrong — the transport's value is fixed for a given build
// — but it would conflate variants the day it stops being fixed. Filed as
// `T-NET-EFFECTIVE-REQUEST-HEADERS-1`.
std::vector<std::string> selecting_header_values(const std::vector<std::string>& names,
                                                 const HttpHeaders& request_headers);

// Headers that must never be written into a cache entry, whatever the server
// said. `Set-Cookie` is the reason this exists: a stored copy would keep a
// session token alive in the cache for reuse, and serving it back would resurrect
// a cookie the user had deleted. Stripping it lets the RESPONSE be cached
// without its per-user state coming along — strictly better than refusing to
// cache such responses at all, which is what 9.3.1 did.
bool is_uncacheable_response_header(std::string_view name);

}  // namespace Hummingbird::Core
