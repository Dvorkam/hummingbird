#pragma once

#include <string_view>

namespace Hummingbird::Core {

// Public suffix lookup: the boundary between "a registry hands these out" and
// "one party owns this". Cookie scoping needs it so a page on `a.co.uk` cannot
// set a cookie for `co.uk` and read it on `b.co.uk`, and M9's CORS credentials
// decisions lean on the same notion of site.
//
// SCOPE: this is a *curated* multi-label suffix list, not the full ICANN public
// suffix list (decision recorded in doc/milestones/milestone9.md, story
// 9.0.3.1). Every single-label TLD is a public suffix implicitly, via the PSL's
// default `*` rule, so the table only carries the multi-label registries and
// hosting suffixes that real browsing hits. It follows the PSL matching
// algorithm exactly — longest match wins, `*` matches one label, `!` is an
// exception — so swapping in the full list later is a data change, not a code
// change. Hosts under an unlisted multi-label registry fall back to "the last
// label is the suffix", which is the pre-9.0.3.1 behavior for exactly those
// hosts and no others. See T-COOKIE-PUBLIC-SUFFIX-FULL-LIST-1.

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
