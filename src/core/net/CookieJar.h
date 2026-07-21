#pragma once

#include <filesystem>
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
// SCOPE (8.1.2): domain/path matching, expiry, Secure, HttpOnly, and SameSite.
// The caller supplies a CookieRequestContext describing who is asking; the
// default describes a user-initiated top-level GET, for which every cookie is
// eligible.
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
    std::vector<Cookie> cookies_for(std::string_view request_url, CookieTime now,
                                    const CookieRequestContext& context = {}) const;

    // The `Cookie` request header value ("a=1; b=2"), or "" when nothing matches.
    std::string cookie_header_for(std::string_view request_url, CookieTime now,
                                  const CookieRequestContext& context = {}) const;

    // The `document.cookie` view: same-origin, non-HttpOnly cookies only.
    // Story 8.1.5 binds this to JS; it lives here so the filter has one owner.
    std::string script_visible_cookies(std::string_view document_url, CookieTime now) const;

    // Drops every cookie whose expiry has passed.
    size_t purge_expired(CookieTime now);

    // --- persistence (story 8.1.4) ------------------------------------------
    // Session cookies are never written: "dies with the process" is their whole
    // definition, so persisting them would silently upgrade them.

    // HB_COOKIES_FILE if set, else a file under the per-profile config dir.
    static std::filesystem::path default_path();

    // Writes every non-session cookie that has not already expired, replacing
    // the file's contents. Returns how many were written. Best-effort: logs and
    // returns 0 if the file cannot be opened. `now` is a parameter for the same
    // reason it is everywhere else here — so the decision is testable.
    size_t save_to(const std::filesystem::path& path, CookieTime now) const;

    // Replaces the jar's contents from `path`, dropping cookies that already
    // expired. Returns how many were loaded. A missing file is normal (first
    // run) and a corrupt one starts empty with a log rather than failing — a
    // browser must still start when its cookie file is damaged.
    size_t load_from(const std::filesystem::path& path, CookieTime now);

    const std::vector<Cookie>& entries() const { return cookies_; }
    size_t size() const { return cookies_.size(); }
    bool empty() const { return cookies_.empty(); }
    void clear() { cookies_.clear(); }

private:
    std::vector<Cookie> cookies_;
};

}  // namespace Hummingbird::Core
