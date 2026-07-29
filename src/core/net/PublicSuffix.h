#pragma once

#include <string_view>

namespace Hummingbird::Core {

// Public suffix lookup: the boundary between "a registry hands these out" and
// "one party owns this". Cookie scoping needs it so a page on `a.co.uk` cannot
// set a cookie for `co.uk` and read it on `b.co.uk`, and M9's CORS credentials
// decisions lean on the same notion of site.
//
// The rules are the full publicsuffix.org list (ICANN + PRIVATE), generated into
// `PublicSuffixData.h` and BUNDLED at build time — never fetched at runtime. A
// browser that must reach the network before it can evaluate cookie policy has
// a bootstrap problem on its first offline start, and fetching a security input
// over the network makes whoever can intercept it an author of your cookie
// boundaries. Chrome and Firefox bundle it for the same reasons.
//
// The list is pinned to an upstream commit and refreshed by CI (a daily job
// opens a PR; a release cannot be tagged while the bundle is behind upstream).
// See `scripts/update_public_suffix_list.ps1`.
//
// Matching follows the published algorithm: an exception rule beats everything,
// then longest match wins, then the implicit `*` default rule makes the last
// label the suffix. Lookup is O(labels), not O(rules) — each candidate suffix is
// binary-searched in a sorted table.

// The public suffix of `host`, as a view into `host` — "co.uk" for
// "www.example.co.uk", "com" for "example.com". Empty for an IP literal or an
// empty host. A host that is itself a public suffix returns the whole host.
// Comparison is case-insensitive; the returned view keeps `host`'s own case.
std::string_view public_suffix(std::string_view host);

// True when `host` is exactly a public suffix, so nothing may be scoped to it.
bool is_public_suffix(std::string_view host);

// The registrable domain of `host`: its public suffix plus one more label —
// "example.co.uk" for "www.example.co.uk". A view into `host`. Empty when there
// is none: an IP literal, or a host that IS a public suffix (nothing is
// registrable at that level).
std::string_view registrable_domain(std::string_view host);

}  // namespace Hummingbird::Core
