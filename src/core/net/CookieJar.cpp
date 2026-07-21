#include "core/net/CookieJar.h"

#include <algorithm>

#include "core/utils/StringUtils.h"
#include "core/utils/Url.h"

namespace Hummingbird::Core {

namespace {

bool scheme_is_secure(std::string_view scheme) { return Utils::equals_ignore_case(scheme, "https"); }

}  // namespace

bool CookieJar::store_from_header(std::string_view request_url, std::string_view set_cookie_value, CookieTime now) {
    auto parsed = parse_set_cookie(set_cookie_value, request_url, now);
    if (!parsed) {
        return false;
    }

    // §5.3 step 11: a same-identity cookie replaces the stored one. Preserve the
    // original creation time so the send-order tiebreak stays stable across a
    // refresh, which is what makes ordering deterministic for a session cookie
    // the server keeps re-setting.
    auto existing = std::find_if(cookies_.begin(), cookies_.end(),
                                 [&](const Cookie& cookie) { return cookie.same_identity_as(*parsed); });
    if (existing != cookies_.end()) {
        parsed->created = existing->created;
        *existing = std::move(*parsed);
        // An update that lands already-expired (Max-Age=0, or a past Expires) is
        // how servers delete a cookie.
        if (existing->is_expired(now)) {
            cookies_.erase(existing);
        }
        return true;
    }

    if (parsed->is_expired(now)) {
        // Deleting a cookie that is not stored is a no-op, not a stored corpse.
        return true;
    }
    cookies_.push_back(std::move(*parsed));
    return true;
}

size_t CookieJar::store_from_response(std::string_view request_url, const HttpHeaders& headers, CookieTime now) {
    size_t accepted = 0;
    for (std::string_view value : headers.get_all("Set-Cookie")) {
        if (store_from_header(request_url, value, now)) {
            ++accepted;
        }
    }
    return accepted;
}

std::vector<Cookie> CookieJar::cookies_for(std::string_view request_url, CookieTime now,
                                           const CookieRequestContext& context) const {
    auto request = parse_absolute_url(request_url);
    if (!request) {
        return {};
    }
    const std::string path = cookie_path_of_url(request_url);
    const bool secure_transport = scheme_is_secure(request->scheme);

    std::vector<Cookie> matched;
    for (const auto& cookie : cookies_) {
        if (cookie.is_expired(now)) continue;
        // HttpOnly exists precisely so an XSS payload cannot read the session
        // cookie, so this filter is the whole point of the flag.
        if (cookie.http_only && context.script_access) continue;
        if (!same_site_allows(cookie, request->host, context)) continue;
        // A host-only cookie goes back to exactly the host that set it; anything
        // else may also travel to subdomains.
        if (cookie.host_only) {
            if (!Utils::equals_ignore_case(request->host, cookie.domain)) continue;
        } else if (!domain_matches(request->host, cookie.domain)) {
            continue;
        }
        if (!path_matches(path, cookie.path)) continue;
        if (cookie.secure && !secure_transport) continue;
        matched.push_back(cookie);
    }

    // §5.4 step 2: longer paths first; equal paths ordered by creation time.
    std::stable_sort(matched.begin(), matched.end(), [](const Cookie& a, const Cookie& b) {
        if (a.path.size() != b.path.size()) {
            return a.path.size() > b.path.size();
        }
        return a.created < b.created;
    });
    return matched;
}

std::string CookieJar::cookie_header_for(std::string_view request_url, CookieTime now,
                                         const CookieRequestContext& context) const {
    std::string header;
    for (const auto& cookie : cookies_for(request_url, now, context)) {
        if (!header.empty()) {
            header += "; ";
        }
        header += cookie.name;
        header += '=';
        header += cookie.value;
    }
    return header;
}

std::string CookieJar::script_visible_cookies(std::string_view document_url, CookieTime now) const {
    CookieRequestContext context;
    context.script_access = true;
    // JS reads its own document's cookies, so the initiator is the document
    // itself: always same-site, never a cross-site read.
    if (auto parts = parse_absolute_url(document_url)) {
        context.initiator_host = parts->host;
    }
    return cookie_header_for(document_url, now, context);
}

size_t CookieJar::purge_expired(CookieTime now) {
    const size_t before = cookies_.size();
    cookies_.erase(std::remove_if(cookies_.begin(), cookies_.end(),
                                  [&](const Cookie& cookie) { return cookie.is_expired(now); }),
                   cookies_.end());
    return before - cookies_.size();
}

}  // namespace Hummingbird::Core
