#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <string_view>

namespace Hummingbird::Core {

// Cookie expiry is wall-clock, not monotonic: it is compared against dates a
// server sent and (from 8.1.4) survives a restart on disk. Core::Clock is
// steady_clock and deliberately not used here.
using CookieClock = std::chrono::system_clock;
using CookieTime = CookieClock::time_point;

enum class SameSite { None, Lax, Strict };

// One stored cookie, normalized per RFC 6265 §5.3.
struct Cookie {
    std::string name;
    std::string value;
    // Lowercase, never carries the leading dot a server may have sent.
    std::string domain;
    // Always begins with '/'.
    std::string path;
    // nullopt means a session cookie: it dies with the process rather than at a
    // date, and 8.1.4 must not persist it.
    std::optional<CookieTime> expires;
    CookieTime created{};
    // Set when the Set-Cookie carried no Domain attribute: the cookie then goes
    // back only to the exact host that set it, never to a subdomain.
    bool host_only = false;
    bool secure = false;
    bool http_only = false;
    SameSite same_site = SameSite::Lax;

    bool is_session() const { return !expires.has_value(); }
    bool is_expired(CookieTime now) const { return expires.has_value() && *expires <= now; }
    // Identity per RFC 6265 §5.3: a later Set-Cookie with the same triple
    // replaces the stored one rather than adding a second.
    bool same_identity_as(const Cookie& other) const {
        return name == other.name && domain == other.domain && path == other.path;
    }
};

// RFC 6265 §5.1.1 cookie-date parsing. Deliberately lenient and
// delimiter-tokenized rather than format-matched, because servers send both
// "Wed, 09 Jun 2021 10:18:14 GMT" and "Wed, 09-Jun-2021 10:18:14 GMT".
// Returns nullopt when no valid date can be recovered.
std::optional<CookieTime> parse_cookie_date(std::string_view input);

// RFC 6265 §5.1.3. True when `host` is `domain` or a subdomain of it. An IP
// literal only ever matches itself.
bool domain_matches(std::string_view host, std::string_view domain);

// RFC 6265 §5.1.4. True when `cookie_path` covers `request_path`.
bool path_matches(std::string_view request_path, std::string_view cookie_path);

// RFC 6265 §5.1.4 default-path: the request path up to (not including) its last
// '/', or "/" when there is no directory component.
std::string default_cookie_path(std::string_view request_path);

// The path component of `url` with any query and fragment removed. Core's
// UrlParts::path bundles all three together, which cookie path-matching must not
// see.
std::string cookie_path_of_url(std::string_view url);

// Parses one `Set-Cookie` field value as received from `request_url`.
// Returns nullopt when the cookie must be ignored: no '=', an empty name, or a
// Domain attribute the requesting host is not allowed to set (§5.3 step 6).
std::optional<Cookie> parse_set_cookie(std::string_view header_value, std::string_view request_url, CookieTime now);

}  // namespace Hummingbird::Core
