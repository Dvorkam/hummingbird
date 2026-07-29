#include "core/net/CacheControl.h"

#include <algorithm>
#include <vector>

#include "core/net/Cookie.h"
#include "core/utils/StringUtils.h"

namespace Hummingbird::Core {

namespace {

// A directive's delta-seconds argument. Returns nullopt for anything that is not
// a run of digits, per RFC 9111 §1.2.2 — a non-numeric max-age is not "zero",
// it is a directive we did not understand, and treating it as zero would make a
// typo silently disable caching for a whole site.
std::optional<long> parse_delta_seconds(std::string_view value) {
    value = Utils::trim_ascii_whitespace(value);
    // Servers do quote these even though the grammar does not allow it.
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
        value = value.substr(1, value.size() - 2);
    }
    if (value.empty()) return std::nullopt;
    long result = 0;
    for (const char c : value) {
        if (c < '0' || c > '9') return std::nullopt;
        // Saturate rather than overflow: `max-age=99999999999999` means "a very
        // long time", and wrapping it into a negative would mean the opposite.
        if (result > 100000000) return 100000000;
        result = result * 10 + (c - '0');
    }
    return result;
}

// Splits on commas that are not inside a quoted string. `Cache-Control` values
// are comma-separated, but a quoted argument may contain one.
std::vector<std::string_view> split_directives(std::string_view value) {
    std::vector<std::string_view> out;
    bool in_quotes = false;
    size_t start = 0;
    for (size_t i = 0; i <= value.size(); ++i) {
        if (i < value.size() && value[i] == '"') {
            in_quotes = !in_quotes;
            continue;
        }
        if (i == value.size() || (value[i] == ',' && !in_quotes)) {
            auto token = Utils::trim_ascii_whitespace(value.substr(start, i - start));
            if (!token.empty()) out.push_back(token);
            start = i + 1;
        }
    }
    return out;
}

// RFC 9111 §3: responses a cache may store when the server states freshness.
//
// 206 is excluded on purpose — it is a byte range, and this engine does not do
// ranges, so storing a partial body as though it were the whole resource would
// hand the next request a truncated file. 405/414/501 are excluded because
// caching an error the operator is probably fixing right now buys nothing.
bool is_cacheable_status(long status) {
    switch (status) {
        case 200:
        case 203:
        case 204:
        case 300:
        case 301:
        case 308:
        case 404:
        case 410:
            return true;
        default:
            return false;
    }
}

}  // namespace

CacheControl parse_cache_control(std::string_view value) {
    CacheControl control;
    for (const auto directive : split_directives(value)) {
        const size_t equals = directive.find('=');
        const std::string name = Utils::to_lower(Utils::trim_ascii_whitespace(
            directive.substr(0, equals == std::string_view::npos ? directive.size() : equals)));
        const std::string_view argument =
            equals == std::string_view::npos ? std::string_view{} : directive.substr(equals + 1);

        if (name == "no-store") {
            control.no_store = true;
        } else if (name == "no-cache") {
            // The field-name form (`no-cache="Set-Cookie"`) means only those
            // headers must not be reused. This engine does not store Set-Cookie
            // responses at all, so treating the qualified form as the blanket
            // one is strictly safer, never less.
            control.no_cache = true;
        } else if (name == "must-revalidate") {
            control.must_revalidate = true;
        } else if (name == "private") {
            control.is_private = true;
        } else if (name == "public") {
            control.is_public = true;
        } else if (name == "immutable") {
            control.immutable = true;
        } else if (name == "max-age") {
            control.max_age = parse_delta_seconds(argument);
        } else if (name == "s-maxage") {
            control.s_maxage = parse_delta_seconds(argument);
        }
    }
    return control;
}

std::string_view describe(Storability storability) {
    switch (storability) {
        case Storability::Storable:
            return "storable";
        case Storability::MethodNotCacheable:
            return "only GET responses are cached";
        case Storability::StatusNotCacheable:
            return "status is not cacheable";
        case Storability::NoStore:
            return "Cache-Control: no-store";
        case Storability::Private:
            return "Cache-Control: private";
        case Storability::HasSetCookie:
            return "response carries Set-Cookie";
        case Storability::RequestHadCredentials:
            return "request carried credentials";
        case Storability::HasVary:
            return "response carries Vary (see story 9.3.2)";
        case Storability::NothingToReuse:
            return "stale on arrival with no validator";
        case Storability::TooLarge:
            return "larger than the per-entry cap";
    }
    return "not storable";
}

Storability storability(std::string_view method, long status, const HttpHeaders& request_headers,
                        const HttpHeaders& response_headers) {
    if (!Utils::equals_ignore_case(method, "GET")) {
        return Storability::MethodNotCacheable;
    }
    if (!is_cacheable_status(status)) {
        return Storability::StatusNotCacheable;
    }

    const CacheControl control = parse_cache_control(response_headers.get("Cache-Control"));
    if (control.no_store) {
        return Storability::NoStore;
    }
    // Pragma: no-cache is HTTP/1.0's no-store-ish ancestor. Honored only when
    // there is no Cache-Control to defer to, which is what RFC 9111 §5.4 says:
    // a server that sent both meant the modern one.
    if (!response_headers.contains("Cache-Control") &&
        Utils::to_lower(response_headers.get("Pragma")).find("no-cache") != std::string::npos) {
        return Storability::NoStore;
    }
    if (control.is_private) {
        return Storability::Private;
    }
    if (response_headers.contains("Set-Cookie") || response_headers.contains("Set-Cookie2")) {
        return Storability::HasSetCookie;
    }
    if (request_headers.contains("Cookie") || request_headers.contains("Authorization")) {
        return Storability::RequestHadCredentials;
    }
    if (response_headers.contains("Vary")) {
        return Storability::HasVary;
    }
    return Storability::Storable;
}

Freshness compute_freshness(const HttpHeaders& response_headers, CacheTime received_at) {
    Freshness freshness;
    const CacheControl control = parse_cache_control(response_headers.get("Cache-Control"));
    freshness.always_revalidate = control.no_cache;

    // The same lenient parser cookies use. `Expires`, `Date` and `Last-Modified`
    // are IMF-fixdate, which that grammar already covers as a subset — writing a
    // second date parser here would be a second place for the same bugs.
    const auto date = parse_cookie_date(response_headers.get("Date"));

    // RFC 9111 §4.2.1, in precedence order. Note s-maxage is NOT consulted: see
    // the header for why that one matters more than it looks.
    if (control.max_age) {
        freshness.lifetime = std::chrono::seconds(*control.max_age);
    } else if (response_headers.contains("Expires")) {
        // A present-but-unparseable `Expires` (servers really do send `0` and
        // `-1`) means "already expired", which falls out of leaving the lifetime
        // at zero — the same place an absent one lands.
        if (const auto expires = parse_cookie_date(response_headers.get("Expires"))) {
            const auto basis = date.value_or(received_at);
            if (*expires > basis) {
                freshness.lifetime = std::chrono::duration_cast<std::chrono::seconds>(*expires - basis);
            }
        }
    }

    // RFC 9111 §4.2.3. `Age` is what a CDN already spent; apparent age is what
    // the network spent getting here. Take the larger, because either alone can
    // be understated (a proxy that omits Age, or a clock skew that hides transit).
    std::chrono::seconds age{0};
    if (const auto age_value = parse_delta_seconds(response_headers.get("Age"))) {
        age = std::chrono::seconds(*age_value);
    }
    if (date && received_at > *date) {
        const auto apparent = std::chrono::duration_cast<std::chrono::seconds>(received_at - *date);
        age = std::max(age, apparent);
    }
    freshness.initial_age = age;
    return freshness;
}

Validators extract_validators(const HttpHeaders& response_headers) {
    Validators validators;
    validators.etag = std::string(Utils::trim_ascii_whitespace(response_headers.get("ETag")));
    validators.last_modified = std::string(Utils::trim_ascii_whitespace(response_headers.get("Last-Modified")));
    return validators;
}

}  // namespace Hummingbird::Core
