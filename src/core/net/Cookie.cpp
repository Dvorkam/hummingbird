#include "core/net/Cookie.h"

#include <algorithm>
#include <cctype>

#include "core/utils/ParseUtils.h"
#include "core/utils/StringUtils.h"
#include "core/utils/Url.h"

namespace Hummingbird::Core {

namespace {

bool is_digit(char c) { return c >= '0' && c <= '9'; }

// RFC 6265 §5.1.1 delimiter set: everything that is not a date character.
bool is_date_delimiter(char c) {
    if (c == 0x09) return true;
    const auto u = static_cast<unsigned char>(c);
    return (u >= 0x20 && u <= 0x2F) || (u >= 0x3B && u <= 0x40) || (u >= 0x5B && u <= 0x60) ||
           (u >= 0x7B && u <= 0x7E);
}

// A host that is an IP literal can only ever match itself, so the subdomain
// suffix rule must not apply to it. IPv6 arrives bracketed or colon-bearing;
// IPv4 is digits and dots only.
bool is_ip_literal(std::string_view host) {
    if (host.find(':') != std::string_view::npos) return true;
    return !host.empty() &&
           std::all_of(host.begin(), host.end(), [](char c) { return is_digit(c) || c == '.'; }) &&
           std::any_of(host.begin(), host.end(), is_digit);
}

int month_from_name(std::string_view token) {
    static constexpr std::string_view kMonths[] = {"jan", "feb", "mar", "apr", "may", "jun",
                                                   "jul", "aug", "sep", "oct", "nov", "dec"};
    if (token.size() < 3) return 0;
    const std::string prefix = Utils::to_lower(token.substr(0, 3));
    for (int i = 0; i < 12; ++i) {
        if (prefix == kMonths[i]) return i + 1;
    }
    return 0;
}

// Parses "h:m:s" out of a token, per the RFC's time production.
bool parse_time_token(std::string_view token, int& hour, int& minute, int& second) {
    const size_t first = token.find(':');
    if (first == std::string_view::npos) return false;
    const size_t next = token.find(':', first + 1);
    if (next == std::string_view::npos) return false;

    auto h = Utils::parse_long(token.substr(0, first), Utils::NumberParseMode::Strict);
    auto m = Utils::parse_long(token.substr(first + 1, next - first - 1), Utils::NumberParseMode::Strict);
    auto s = Utils::parse_long(token.substr(next + 1), Utils::NumberParseMode::Strict);
    if (!h || !m || !s) return false;
    if (*h > 23 || *m > 59 || *s > 59 || *h < 0 || *m < 0 || *s < 0) return false;
    hour = static_cast<int>(*h);
    minute = static_cast<int>(*m);
    second = static_cast<int>(*s);
    return true;
}

std::string_view trim(std::string_view input) { return Utils::trim_ascii_whitespace(input); }

// Splits "name=value"; a missing '=' yields an empty name so callers can reject.
std::pair<std::string_view, std::string_view> split_pair(std::string_view input) {
    const size_t eq = input.find('=');
    if (eq == std::string_view::npos) {
        return {trim(input), {}};
    }
    return {trim(input.substr(0, eq)), trim(input.substr(eq + 1))};
}

}  // namespace

std::optional<CookieTime> parse_cookie_date(std::string_view input) {
    int hour = -1, minute = -1, second = -1;
    int day = -1, month = 0, year = -1;

    size_t pos = 0;
    while (pos < input.size()) {
        while (pos < input.size() && is_date_delimiter(input[pos])) ++pos;
        const size_t start = pos;
        while (pos < input.size() && !is_date_delimiter(input[pos])) ++pos;
        const std::string_view token = input.substr(start, pos - start);
        if (token.empty()) continue;

        if (hour < 0 && parse_time_token(token, hour, minute, second)) {
            continue;
        }
        if (day < 0 && is_digit(token[0]) && token.size() <= 2) {
            if (auto value = Utils::parse_long(token, Utils::NumberParseMode::Strict); value && *value >= 1 && *value <= 31) {
                day = static_cast<int>(*value);
                continue;
            }
        }
        if (month == 0) {
            if (const int parsed = month_from_name(token); parsed != 0) {
                month = parsed;
                continue;
            }
        }
        if (year < 0 && is_digit(token[0])) {
            if (auto value = Utils::parse_long(token, Utils::NumberParseMode::Strict); value && *value >= 0) {
                year = static_cast<int>(*value);
                continue;
            }
        }
    }

    if (day < 0 || month == 0 || year < 0 || hour < 0) {
        return std::nullopt;
    }
    // RFC 6265 §5.1.1: two-digit years map into 1970-2069.
    if (year >= 70 && year <= 99) {
        year += 1900;
    } else if (year >= 0 && year <= 69) {
        year += 2000;
    }
    if (year < 1601) {
        return std::nullopt;
    }

    const std::chrono::year_month_day ymd{std::chrono::year{year}, std::chrono::month{static_cast<unsigned>(month)},
                                          std::chrono::day{static_cast<unsigned>(day)}};
    if (!ymd.ok()) {
        return std::nullopt;
    }
    return CookieTime{std::chrono::sys_days{ymd}} + std::chrono::hours{hour} + std::chrono::minutes{minute} +
           std::chrono::seconds{second};
}

bool domain_matches(std::string_view host, std::string_view domain) {
    if (host.empty() || domain.empty()) return false;
    if (Utils::equals_ignore_case(host, domain)) return true;
    // Only a real subdomain matches, and never for an IP literal.
    if (is_ip_literal(host)) return false;
    if (host.size() <= domain.size()) return false;
    const size_t offset = host.size() - domain.size();
    if (host[offset - 1] != '.') return false;
    return Utils::equals_ignore_case(host.substr(offset), domain);
}

bool path_matches(std::string_view request_path, std::string_view cookie_path) {
    if (cookie_path.empty()) return false;
    if (request_path == cookie_path) return true;
    if (request_path.size() <= cookie_path.size()) return false;
    if (request_path.substr(0, cookie_path.size()) != cookie_path) return false;
    // "/foo" covers "/foo/bar" but must not cover "/foobar".
    return cookie_path.back() == '/' || request_path[cookie_path.size()] == '/';
}

std::string default_cookie_path(std::string_view request_path) {
    if (request_path.empty() || request_path.front() != '/') {
        return "/";
    }
    const size_t last_slash = request_path.find_last_of('/');
    if (last_slash == 0) {
        return "/";
    }
    return std::string(request_path.substr(0, last_slash));
}

std::string cookie_path_of_url(std::string_view url) {
    auto parts = parse_absolute_url(url);
    if (!parts) {
        return "/";
    }
    std::string_view path{parts->path};
    if (const size_t cut = path.find_first_of("?#"); cut != std::string_view::npos) {
        path = path.substr(0, cut);
    }
    if (path.empty()) {
        return "/";
    }
    return std::string(path);
}

std::optional<Cookie> parse_set_cookie(std::string_view header_value, std::string_view request_url, CookieTime now) {
    auto request = parse_absolute_url(request_url);
    if (!request) {
        return std::nullopt;
    }

    // §5.2: the name-value pair is everything before the first ';'.
    const size_t first_semi = header_value.find(';');
    const std::string_view pair = header_value.substr(0, first_semi);
    auto [name, value] = split_pair(pair);
    // A Set-Cookie with no '=' at all, or an empty name, is ignored entirely.
    if (name.empty() || pair.find('=') == std::string_view::npos) {
        return std::nullopt;
    }

    Cookie cookie;
    cookie.name = std::string(name);
    cookie.value = std::string(value);
    cookie.created = now;

    // Max-Age wins over Expires (§5.3 step 3), so track them separately and
    // resolve after the whole attribute list is read.
    std::optional<CookieTime> expires_attr;
    std::optional<long> max_age_attr;
    std::string domain_attr;
    std::string path_attr;

    size_t pos = first_semi == std::string_view::npos ? header_value.size() : first_semi + 1;
    while (pos < header_value.size()) {
        const size_t semi = header_value.find(';', pos);
        const std::string_view chunk =
            semi == std::string_view::npos ? header_value.substr(pos) : header_value.substr(pos, semi - pos);
        pos = semi == std::string_view::npos ? header_value.size() : semi + 1;

        auto [attr_name, attr_value] = split_pair(chunk);
        if (attr_name.empty()) continue;

        if (Utils::equals_ignore_case(attr_name, "expires")) {
            expires_attr = parse_cookie_date(attr_value);
        } else if (Utils::equals_ignore_case(attr_name, "max-age")) {
            max_age_attr = Utils::parse_long(attr_value, Utils::NumberParseMode::Strict);
        } else if (Utils::equals_ignore_case(attr_name, "domain")) {
            domain_attr = Utils::to_lower(attr_value);
        } else if (Utils::equals_ignore_case(attr_name, "path")) {
            path_attr = std::string(attr_value);
        } else if (Utils::equals_ignore_case(attr_name, "secure")) {
            cookie.secure = true;
        } else if (Utils::equals_ignore_case(attr_name, "httponly")) {
            cookie.http_only = true;
        } else if (Utils::equals_ignore_case(attr_name, "samesite")) {
            if (Utils::equals_ignore_case(attr_value, "strict")) {
                cookie.same_site = SameSite::Strict;
            } else if (Utils::equals_ignore_case(attr_value, "none")) {
                cookie.same_site = SameSite::None;
            } else {
                cookie.same_site = SameSite::Lax;
            }
        }
    }

    if (max_age_attr.has_value()) {
        // A zero or negative Max-Age means "expire now", which the jar turns into
        // a removal.
        cookie.expires = now + std::chrono::seconds{*max_age_attr};
    } else if (expires_attr.has_value()) {
        cookie.expires = *expires_attr;
    }

    // §5.3 steps 5-6: a Domain the request host is not within is rejected
    // outright, so a page cannot set a cookie for an unrelated site.
    if (!domain_attr.empty()) {
        if (domain_attr.front() == '.') {
            domain_attr.erase(0, 1);
        }
    }
    if (domain_attr.empty()) {
        cookie.host_only = true;
        cookie.domain = request->host;
    } else {
        if (!domain_matches(request->host, domain_attr)) {
            return std::nullopt;
        }
        cookie.host_only = false;
        cookie.domain = domain_attr;
    }

    cookie.path = (path_attr.empty() || path_attr.front() != '/') ? default_cookie_path(cookie_path_of_url(request_url))
                                                                  : path_attr;

    // RFC 6265bis §5.4: SameSite=None is only meaningful on a Secure cookie, and
    // announcing cross-site availability without it is rejected rather than
    // silently downgraded to Lax — a silent downgrade would look like it worked.
    if (cookie.same_site == SameSite::None && !cookie.secure) {
        return std::nullopt;
    }
    return cookie;
}

bool is_same_site(std::string_view request_host, std::string_view initiator_host) {
    // No initiator: a user-typed URL or bookmark, which is not a cross-site
    // request at all.
    if (initiator_host.empty()) return true;
    if (Utils::equals_ignore_case(request_host, initiator_host)) return true;

    // Approximate the registrable domain as the last two labels. See the header
    // for why this is wrong under multi-label public suffixes.
    const auto registrable = [](std::string_view host) -> std::string_view {
        size_t last_dot = host.find_last_of('.');
        if (last_dot == std::string_view::npos) return host;
        size_t prev_dot = host.find_last_of('.', last_dot - 1);
        if (prev_dot == std::string_view::npos) return host;
        return host.substr(prev_dot + 1);
    };
    return Utils::equals_ignore_case(registrable(request_host), registrable(initiator_host));
}

bool same_site_allows(const Cookie& cookie, std::string_view request_host, const CookieRequestContext& context) {
    if (cookie.same_site == SameSite::None) {
        return true;  // explicitly cross-site (and Secure, enforced at parse).
    }
    if (is_same_site(request_host, context.initiator_host)) {
        return true;
    }
    // Cross-site from here on.
    if (cookie.same_site == SameSite::Strict) {
        return false;
    }
    // Lax rides a cross-site request only as a top-level navigation with a safe
    // method: following a link carries your session, a POST or a subresource
    // fetch does not.
    return context.top_level_navigation && context.safe_method;
}

}  // namespace Hummingbird::Core
