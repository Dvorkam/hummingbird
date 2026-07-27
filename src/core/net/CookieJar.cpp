#include "core/net/CookieJar.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <sstream>

#include "core/utils/AssetPath.h"
#include "core/utils/Log.h"
#include "core/utils/ParseUtils.h"
#include "core/utils/StringUtils.h"
#include "core/utils/Url.h"

namespace Hummingbird::Core {

namespace {

bool scheme_is_secure(std::string_view scheme) {
    return Utils::equals_ignore_case(scheme, "https");
}

}  // namespace

bool CookieJar::store_from_header(std::string_view request_url, std::string_view set_cookie_value, CookieTime now) {
    auto parsed = parse_set_cookie(set_cookie_value, request_url, now);
    if (!parsed) {
        return false;
    }

    // §6.1: refuse an oversized cookie outright rather than store it and evict
    // something real to make room. Applies to a replacement too — a cookie may
    // not grow past the cap by being re-set.
    if (parsed->name.size() + parsed->value.size() > kMaxCookieBytes) {
        HB_LOG_DEBUG("[cookies] refusing oversized cookie: " << parsed->name);
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
    evict_for(parsed->domain, now);
    cookies_.push_back(std::move(*parsed));
    return true;
}

namespace {
// RFC 6265 §5.3 step 11's ordering: the least-recently-used cookie, with
// creation time breaking a tie so eviction is deterministic under test (two
// cookies stored at the same `now` are otherwise indistinguishable).
bool less_recently_used(const Cookie& a, const Cookie& b) {
    if (a.last_access != b.last_access) return a.last_access < b.last_access;
    return a.created < b.created;
}
}  // namespace

size_t CookieJar::evict_for(std::string_view incoming_domain, CookieTime now) {
    const auto same_domain = [&](const Cookie& cookie) {
        return Utils::equals_ignore_case(cookie.domain, incoming_domain);
    };
    const size_t in_domain = static_cast<size_t>(std::count_if(cookies_.begin(), cookies_.end(), same_domain));
    if (in_domain < kMaxPerDomain && cookies_.size() < kMaxTotal) {
        return 0;  // room for one more on both counts: nothing to do
    }

    // Step 11's first pass: expired cookies are evicted before any live one.
    size_t dropped = purge_expired(now);

    // Per-domain cap, then the global one. Each loop leaves room for the
    // incoming cookie, hence `>=` rather than `>`.
    while (static_cast<size_t>(std::count_if(cookies_.begin(), cookies_.end(), same_domain)) >= kMaxPerDomain) {
        auto victim = cookies_.end();
        for (auto it = cookies_.begin(); it != cookies_.end(); ++it) {
            if (!same_domain(*it)) continue;
            if (victim == cookies_.end() || less_recently_used(*it, *victim)) victim = it;
        }
        if (victim == cookies_.end()) break;
        cookies_.erase(victim);
        ++dropped;
    }
    while (cookies_.size() >= kMaxTotal) {
        auto victim = std::min_element(cookies_.begin(), cookies_.end(), less_recently_used);
        if (victim == cookies_.end()) break;
        cookies_.erase(victim);
        ++dropped;
    }
    return dropped;
}

size_t CookieJar::trim_to_limits(CookieTime now) {
    size_t dropped = 0;
    if (cookies_.size() > kMaxTotal) {
        dropped += purge_expired(now);
    }
    // Per-domain first: a jar can be under the total cap while one domain is far
    // over its own.
    std::vector<std::string> domains;
    for (const auto& cookie : cookies_) {
        if (std::find(domains.begin(), domains.end(), cookie.domain) == domains.end()) {
            domains.push_back(cookie.domain);
        }
    }
    for (const std::string& domain : domains) {
        const auto in_domain = [&](const Cookie& cookie) { return cookie.domain == domain; };
        while (static_cast<size_t>(std::count_if(cookies_.begin(), cookies_.end(), in_domain)) > kMaxPerDomain) {
            auto victim = cookies_.end();
            for (auto it = cookies_.begin(); it != cookies_.end(); ++it) {
                if (!in_domain(*it)) continue;
                if (victim == cookies_.end() || less_recently_used(*it, *victim)) victim = it;
            }
            if (victim == cookies_.end()) break;
            cookies_.erase(victim);
            ++dropped;
        }
    }
    while (cookies_.size() > kMaxTotal) {
        auto victim = std::min_element(cookies_.begin(), cookies_.end(), less_recently_used);
        if (victim == cookies_.end()) break;
        cookies_.erase(victim);
        ++dropped;
    }
    return dropped;
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
                                           const CookieRequestContext& context) {
    auto request = parse_absolute_url(request_url);
    if (!request) {
        return {};
    }
    const std::string path = cookie_path_of_url(request_url);
    const bool secure_transport = scheme_is_secure(request->scheme);

    std::vector<Cookie> matched;
    for (auto& cookie : cookies_) {
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
        // §5.3: retrieval is what makes a cookie "recently used", and that is the
        // only thing last_access feeds (story 9.0.3.2).
        cookie.last_access = now;
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
                                         const CookieRequestContext& context) {
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

std::string CookieJar::script_visible_cookies(std::string_view document_url, CookieTime now) {
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
    cookies_.erase(
        std::remove_if(cookies_.begin(), cookies_.end(), [&](const Cookie& cookie) { return cookie.is_expired(now); }),
        cookies_.end());
    return before - cookies_.size();
}

// --- persistence (story 8.1.4) ----------------------------------------------

namespace {
// A version tag so a future format change can be detected rather than
// misparsed. Bump it and the old file is discarded as unreadable.
constexpr std::string_view kFileHeader = "HBCOOKIES\t1";

long long to_epoch_seconds(CookieTime time) {
    return std::chrono::duration_cast<std::chrono::seconds>(time.time_since_epoch()).count();
}

CookieTime from_epoch_seconds(long long seconds) {
    return CookieTime{} + std::chrono::seconds{seconds};
}

// The TSV format cannot represent a field containing a tab or newline. Since
// story 9.0.3.3, parse_set_cookie rejects those bytes outright and load_from
// cannot produce them (it splits on exactly those characters), so nothing should
// ever fail this check — it stays as a guard on the serialization boundary,
// where the cost of being wrong is a corrupt cookie file rather than one lost
// cookie. Reaching it means a parse bug, hence the warning.
bool is_serializable(const Cookie& cookie) {
    for (const std::string* field : {&cookie.name, &cookie.value, &cookie.domain, &cookie.path}) {
        if (field->find_first_of("\t\r\n") != std::string::npos) return false;
    }
    return true;
}

std::vector<std::string> split_tabs(const std::string& line) {
    std::vector<std::string> parts;
    size_t start = 0;
    while (true) {
        const size_t tab = line.find('\t', start);
        if (tab == std::string::npos) {
            parts.push_back(line.substr(start));
            return parts;
        }
        parts.push_back(line.substr(start, tab - start));
        start = tab + 1;
    }
}
}  // namespace

std::filesystem::path CookieJar::default_path() {
    if (const char* configured = std::getenv("HB_COOKIES_FILE"); configured && configured[0]) {
        return std::filesystem::path(configured);
    }
    return Utils::resolve_asset_path("assets/config/cookies.tsv");
}

size_t CookieJar::save_to(const std::filesystem::path& path, CookieTime now) const {
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        HB_LOG_WARN("[cookies] could not write " << path.string());
        return 0;
    }
    file << kFileHeader << '\n';

    size_t written = 0;
    for (const auto& cookie : cookies_) {
        // Session cookies die with the process by definition.
        if (cookie.is_session()) continue;
        // No point writing a corpse: load_from would drop it anyway, and it
        // would otherwise sit in the file until the next clean shutdown.
        if (cookie.is_expired(now)) continue;
        if (!is_serializable(cookie)) {
            HB_LOG_WARN("[cookies] unserializable cookie reached the jar: " << cookie.name);
            continue;
        }
        file << cookie.name << '\t' << cookie.value << '\t' << cookie.domain << '\t' << cookie.path << '\t'
             << to_epoch_seconds(*cookie.expires) << '\t' << to_epoch_seconds(cookie.created) << '\t'
             << (cookie.host_only ? 1 : 0) << '\t' << (cookie.secure ? 1 : 0) << '\t' << (cookie.http_only ? 1 : 0)
             << '\t' << static_cast<int>(cookie.same_site) << '\n';
        ++written;
    }
    return written;
}

size_t CookieJar::load_from(const std::filesystem::path& path, CookieTime now) {
    cookies_.clear();
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return 0;  // First run: no file yet is entirely normal.
    }

    std::string line;
    if (!std::getline(file, line)) {
        return 0;
    }
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line != kFileHeader) {
        HB_LOG_WARN("[cookies] unrecognized cookie file format, starting empty: " << path.string());
        return 0;
    }

    size_t loaded = 0;
    size_t skipped = 0;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

        const auto parts = split_tabs(line);
        if (parts.size() != 10) {
            ++skipped;
            continue;
        }
        const auto expires = Utils::parse_long(parts[4], Utils::NumberParseMode::Strict);
        const auto created = Utils::parse_long(parts[5], Utils::NumberParseMode::Strict);
        const auto same_site = Utils::parse_long(parts[9], Utils::NumberParseMode::Strict);
        if (parts[0].empty() || !expires || !created || !same_site || *same_site < 0 || *same_site > 2) {
            ++skipped;
            continue;
        }

        Cookie cookie;
        cookie.name = parts[0];
        cookie.value = parts[1];
        cookie.domain = parts[2];
        cookie.path = parts[3];
        cookie.expires = from_epoch_seconds(*expires);
        cookie.created = from_epoch_seconds(*created);
        cookie.host_only = parts[6] == "1";
        cookie.secure = parts[7] == "1";
        cookie.http_only = parts[8] == "1";
        cookie.same_site = static_cast<SameSite>(*same_site);

        // Purge on load: a cookie that expired while the browser was closed must
        // not come back to life.
        if (cookie.is_expired(now)) continue;
        // last_access is not persisted (see Cookie.h): seed it from creation
        // time so eviction order is defined on the first request after a start.
        cookie.last_access = cookie.created;
        cookies_.push_back(std::move(cookie));
        ++loaded;
    }

    if (skipped > 0) {
        HB_LOG_WARN("[cookies] skipped " << skipped << " malformed line(s) in " << path.string());
    }
    // The file is as untrusted as the network that filled it — an oversized jar
    // on disk must not become an oversized jar in memory (§6.1, story 9.0.3.2).
    if (const size_t dropped = trim_to_limits(now); dropped > 0) {
        HB_LOG_WARN("[cookies] dropped " << dropped << " cookie(s) over the storage limits on load");
        loaded -= std::min(loaded, dropped);
    }
    return loaded;
}

}  // namespace Hummingbird::Core
