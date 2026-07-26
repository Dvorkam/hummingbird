#pragma once

#include <string>
#include <vector>

namespace Hummingbird::Core {

// How the browser presents itself to a site. Only the legacy `User-Agent` string
// differs between the two modes; the `Sec-CH-UA` client hints are identical and
// truthful in both, so a site always learns it is talking to Hummingbird.
//
//   Transparent   — honest UA ("Hummingbird/<version> ..."). The default.
//   Compatibility — a canonical Chrome-shaped UA, for sites (e.g. Hacker News)
//                   whose anti-crawler rule rejects any noncanonical UA. Opt-in
//                   per origin, never automatic, never for a POST.
enum class IdentityMode { Transparent, Compatibility };

struct IdentityHeader {
    std::string name;
    std::string value;
};

// The identity request headers for `mode`. `Sec-CH-UA*` hints are only emitted
// on a secure request (browsers gate client hints on HTTPS), so `secure` mirrors
// that. The hints never claim "Chromium" — Hummingbird is not Chromium — and the
// high-entropy `Sec-CH-UA-Full-Version-List` is withheld until an origin asks for
// it via `Accept-CH` (T-NET-CLIENT-HINTS-1, not yet implemented).
std::vector<IdentityHeader> identity_headers(IdentityMode mode, bool secure);

// The honest default User-Agent (Transparent mode), for any path that needs a UA
// outside the per-origin identity flow.
std::string transparent_user_agent();

}  // namespace Hummingbird::Core
