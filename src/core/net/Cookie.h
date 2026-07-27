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
    // RFC 6265 §5.3's last-access-time, used only to pick an eviction victim
    // (story 9.0.3.2). Deliberately NOT persisted: adding a field would bump the
    // jar's file format and discard every existing session for the sake of an
    // eviction tiebreak. load_from seeds it from `created`, so LRU order is
    // exact within a run and degrades to creation order across a restart.
    CookieTime last_access{};
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
// Returns nullopt when the cookie must be ignored: no '=', an empty name, a
// Domain attribute the requesting host is not allowed to set (§5.3 step 6), or
// `SameSite=None` without `Secure` (which browsers reject outright).
std::optional<Cookie> parse_set_cookie(std::string_view header_value, std::string_view request_url, CookieTime now);

// Who is asking, which SameSite needs and plain domain/path matching does not
// (story 8.1.2). Default-constructed it describes a user-initiated top-level
// GET — the address-bar case, where every cookie is eligible.
struct CookieRequestContext {
    // A document navigation (address bar, link, form submit) rather than a
    // subresource fetch. Only a top-level navigation can carry a Lax cookie
    // across sites.
    bool top_level_navigation = true;

    // Host of the document that initiated the request. Empty means "no
    // cross-site initiator" — a user-typed URL or a bookmark — which counts as
    // same-site.
    std::string initiator_host;

    // GET/HEAD. A cross-site top-level POST does not carry Lax cookies, which is
    // the login-CSRF case Lax exists to stop.
    bool safe_method = true;

    // Set for `document.cookie` reads: HttpOnly cookies are withheld from JS.
    bool script_access = false;
};

// True when `request_host` and `initiator_host` are same-site: same registrable
// domain, per core/net/PublicSuffix.h (story 9.0.3.1). Hosts with no registrable
// domain — an IP literal, or a host that IS a public suffix — are same-site only
// with themselves.
bool is_same_site(std::string_view request_host, std::string_view initiator_host);

// RFC 6265bis §5.5: whether `cookie`'s SameSite lets it ride this request.
bool same_site_allows(const Cookie& cookie, std::string_view request_host, const CookieRequestContext& context);

}  // namespace Hummingbird::Core
