#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "core/net/Cookie.h"
#include "core/net/HttpHeaders.h"

namespace Hummingbird::Core {

// The engine-owned cookie store (story 8.1.1), single profile.
//
// libcurl's own cookie engine stays off deliberately: keeping the jar here makes
// policy inspectable and testable, and lets `document.cookie` (8.1.5) and the
// persistence format (8.1.4) share one implementation.
//
// `now` is a parameter on every time-sensitive call rather than read from the
// clock internally, so expiry behavior is deterministic under test.
//
// SCOPE (8.1.1): domain/path matching, expiry, and Secure. HttpOnly and SameSite
// are parsed and stored but not yet enforced against a request context — that is
// story 8.1.2, which adds the top-level/subresource and same-site distinctions
// this signature does not carry yet.
class CookieJar {
public:
    // Stores every `Set-Cookie` in `headers` as received from `request_url`.
    // Returns how many were accepted; malformed ones are skipped silently, as a
    // browser must not fail a page load over a bad cookie.
    size_t store_from_response(std::string_view request_url, const HttpHeaders& headers, CookieTime now);

    // Stores one `Set-Cookie` field value. Returns false when it was ignored.
    bool store_from_header(std::string_view request_url, std::string_view set_cookie_value, CookieTime now);

    // Cookies that should be sent to `request_url`, in RFC 6265 §5.4 send order:
    // longer paths first, then earlier creation time.
    std::vector<Cookie> cookies_for(std::string_view request_url, CookieTime now) const;

    // The `Cookie` request header value ("a=1; b=2"), or "" when nothing matches.
    std::string cookie_header_for(std::string_view request_url, CookieTime now) const;

    // Drops every cookie whose expiry has passed.
    size_t purge_expired(CookieTime now);

    const std::vector<Cookie>& entries() const { return cookies_; }
    size_t size() const { return cookies_.size(); }
    bool empty() const { return cookies_.empty(); }
    void clear() { cookies_.clear(); }

private:
    std::vector<Cookie> cookies_;
};

}  // namespace Hummingbird::Core
