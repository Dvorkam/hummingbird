#include "core/net/RequestFilter.h"

#include <algorithm>
#include <array>
#include <cctype>

#include "core/net/PublicSuffix.h"

namespace Hummingbird::Core {
namespace {

char lower(char c) {
    return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
}

// Case-insensitive "does `haystack` contain `needle`". Matching is
// case-insensitive because a URL's host always is, and because a filter list
// author writing "/ads/" does not expect "/Ads/" to slip past — Chrome made
// `urlFilter` case-insensitive by default for the same reason.
bool contains_ci(std::string_view haystack, std::string_view needle) {
    if (needle.empty()) return true;
    if (needle.size() > haystack.size()) return false;
    const auto it = std::search(haystack.begin(), haystack.end(), needle.begin(), needle.end(),
                                [](char a, char b) { return lower(a) == lower(b); });
    return it != haystack.end();
}

bool equals_ci(std::string_view a, std::string_view b) {
    return a.size() == b.size() &&
           std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) { return lower(x) == lower(y); });
}

// True when `host` is `domain` or a subdomain of it. The dot boundary is the
// whole point: a plain suffix test would make a rule for "doubleclick.net"
// block "notdoubleclick.net", which is how naive blockers take down unrelated
// sites.
bool host_matches_domain(std::string_view host, std::string_view domain) {
    if (domain.empty()) return true;
    if (equals_ci(host, domain)) return true;
    if (host.size() <= domain.size() + 1) return false;
    const std::size_t offset = host.size() - domain.size();
    return host[offset - 1] == '.' && equals_ci(host.substr(offset), domain);
}

std::string_view host_of(std::string_view url) {
    // parse_absolute_url returns owned strings, so a view into its result would
    // dangle. Find the authority in `url` itself instead.
    const std::size_t scheme_end = url.find("://");
    if (scheme_end == std::string_view::npos) return {};
    const std::size_t start = scheme_end + 3;
    if (start >= url.size()) return {};
    std::size_t end = url.size();
    for (std::size_t i = start; i < url.size(); ++i) {
        const char c = url[i];
        if (c == '/' || c == '?' || c == '#') {
            end = i;
            break;
        }
    }
    std::string_view authority = url.substr(start, end - start);
    // Strip userinfo, then the port. Userinfo first: `user@host:port` would
    // otherwise have its port stripped from the wrong side of the '@'.
    if (const std::size_t at = authority.rfind('@'); at != std::string_view::npos) {
        authority.remove_prefix(at + 1);
    }
    if (const std::size_t colon = authority.rfind(':'); colon != std::string_view::npos) {
        // Not a port if it is inside an IPv6 literal.
        if (authority.find(']') == std::string_view::npos || colon > authority.find(']')) {
            authority = authority.substr(0, colon);
        }
    }
    return authority;
}

// Same site, by registrable domain rather than by host — `img.example.com`
// fetched from `www.example.com` is first-party. A host with no registrable
// domain (an IP literal, `localhost`) falls back to comparing the host itself,
// which is the strictest reading available and keeps a test fixture on
// `127.0.0.1` from being classed as third-party to itself.
bool is_third_party(std::string_view request_host, std::string_view initiator_host) {
    if (initiator_host.empty()) return false;  // user-initiated counts as first-party
    if (equals_ci(request_host, initiator_host)) return false;
    const std::string_view request_site = registrable_domain(request_host);
    const std::string_view initiator_site = registrable_domain(initiator_host);
    if (request_site.empty() || initiator_site.empty()) return true;
    return !equals_ci(request_site, initiator_site);
}

bool destination_matches(const FilterRule& rule, RequestDestination destination) {
    // An empty list means "every destination this milestone matches", and
    // Document is already excluded before we get here.
    if (rule.destinations.empty()) return true;
    return std::find(rule.destinations.begin(), rule.destinations.end(), destination) != rule.destinations.end();
}

}  // namespace

bool parse_request_destination(std::string_view name, RequestDestination* out) {
    struct Entry {
        std::string_view name;
        RequestDestination destination;
    };
    // The wire names are MV3's, so a filter list written against Chrome's
    // documentation means here what it means there.
    static constexpr std::array<Entry, 6> kNames{{
        {"main_frame", RequestDestination::Document},
        {"stylesheet", RequestDestination::Stylesheet},
        {"image", RequestDestination::Image},
        {"font", RequestDestination::Font},
        {"script", RequestDestination::Script},
        {"xmlhttprequest", RequestDestination::Fetch},
    }};
    for (const auto& entry : kNames) {
        if (equals_ci(name, entry.name)) {
            if (out) *out = entry.destination;
            return true;
        }
    }
    return false;
}

void RequestFilter::set_rules(std::string_view source, std::vector<FilterRule> rules) {
    if (source.empty()) return;
    std::unique_lock lock(mutex_);
    if (rules.empty()) {
        rules_by_source_.erase(std::string(source));
        return;
    }
    rules_by_source_[std::string(source)] = std::move(rules);
}

void RequestFilter::remove_source(std::string_view source) {
    std::unique_lock lock(mutex_);
    rules_by_source_.erase(std::string(source));
}

void RequestFilter::clear() {
    std::unique_lock lock(mutex_);
    rules_by_source_.clear();
}

RequestFilter::Match RequestFilter::match(const Request& request) const {
    // A top-level navigation is never blocked in M9 (see RequestDestination).
    // Checked before the lock: it is not a rule decision, it is a scope limit.
    if (request.destination == RequestDestination::Document) return {};
    if (request.url.empty()) return {};

    const std::string_view host = host_of(request.url);
    if (host.empty()) return {};  // no host to attribute a rule to (data:, about:)

    std::shared_lock lock(mutex_);
    if (rules_by_source_.empty()) return {};

    // `third_party` is computed lazily: it costs a public-suffix lookup, and
    // most rules do not scope by it.
    bool third_party_known = false;
    bool third_party = false;

    for (const auto& [source, rules] : rules_by_source_) {
        for (const auto& rule : rules) {
            if (!rule.constrains_something()) continue;
            if (!destination_matches(rule, request.destination)) continue;
            if (!host_matches_domain(host, rule.request_domain)) continue;
            if (!contains_ci(request.url, rule.url_filter)) continue;
            if (rule.third_party != ThirdPartyScope::Any) {
                if (!third_party_known) {
                    third_party = is_third_party(host, request.initiator_host);
                    third_party_known = true;
                }
                const bool wanted = rule.third_party == ThirdPartyScope::ThirdPartyOnly;
                if (third_party != wanted) continue;
            }
            ++blocked_count_;
            return Match{true, source, rule.id};
        }
    }
    return {};
}

bool RequestFilter::empty() const {
    std::shared_lock lock(mutex_);
    return rules_by_source_.empty();
}

std::size_t RequestFilter::rule_count() const {
    std::shared_lock lock(mutex_);
    std::size_t total = 0;
    for (const auto& [source, rules] : rules_by_source_) {
        total += rules.size();
    }
    return total;
}

std::size_t RequestFilter::source_count() const {
    std::shared_lock lock(mutex_);
    return rules_by_source_.size();
}

std::uint64_t RequestFilter::blocked_count() const {
    std::shared_lock lock(mutex_);
    return blocked_count_;
}

void RequestFilter::reset_counters() {
    std::unique_lock lock(mutex_);
    blocked_count_ = 0;
}

}  // namespace Hummingbird::Core
