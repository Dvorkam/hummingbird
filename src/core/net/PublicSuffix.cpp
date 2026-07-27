#include "core/net/PublicSuffix.h"

#include <algorithm>
#include <array>

#include "core/utils/StringUtils.h"

namespace Hummingbird::Core {

namespace {

// Curated public suffix rules, in publicsuffix.org syntax so the table can be
// diffed against the real list. Single-label TLDs are NOT listed: the PSL's
// implicit `*` default rule already makes every one of them a public suffix.
//
// Selection rule for what earns a line here: a multi-label registry a user of
// this browser plausibly visits, or a hosting suffix under which two unrelated
// sites get subdomains (where cookie scoping is the whole point). Everything
// else waits for the full list.
constexpr std::array kRules{
    // -- ICANN: multi-label country registries -------------------------------
    std::string_view{"co.uk"},
    std::string_view{"org.uk"},
    std::string_view{"me.uk"},
    std::string_view{"ac.uk"},
    std::string_view{"gov.uk"},
    std::string_view{"net.uk"},
    std::string_view{"nhs.uk"},
    std::string_view{"*.sch.uk"},

    std::string_view{"com.au"},
    std::string_view{"net.au"},
    std::string_view{"org.au"},
    std::string_view{"edu.au"},
    std::string_view{"gov.au"},
    std::string_view{"id.au"},

    std::string_view{"co.nz"},
    std::string_view{"net.nz"},
    std::string_view{"org.nz"},
    std::string_view{"govt.nz"},
    std::string_view{"ac.nz"},

    std::string_view{"co.jp"},
    std::string_view{"or.jp"},
    std::string_view{"ne.jp"},
    std::string_view{"ac.jp"},
    std::string_view{"go.jp"},

    std::string_view{"com.br"},
    std::string_view{"net.br"},
    std::string_view{"org.br"},
    std::string_view{"gov.br"},
    std::string_view{"edu.br"},

    std::string_view{"co.in"},
    std::string_view{"net.in"},
    std::string_view{"org.in"},
    std::string_view{"ac.in"},
    std::string_view{"gov.in"},

    std::string_view{"co.za"},
    std::string_view{"org.za"},
    std::string_view{"net.za"},
    std::string_view{"ac.za"},
    std::string_view{"gov.za"},

    std::string_view{"com.cn"},
    std::string_view{"net.cn"},
    std::string_view{"org.cn"},
    std::string_view{"gov.cn"},
    std::string_view{"edu.cn"},

    std::string_view{"co.kr"},
    std::string_view{"or.kr"},
    std::string_view{"ne.kr"},
    std::string_view{"go.kr"},
    std::string_view{"ac.kr"},

    std::string_view{"co.il"},
    std::string_view{"org.il"},
    std::string_view{"ac.il"},
    std::string_view{"gov.il"},

    std::string_view{"com.tr"},
    std::string_view{"net.tr"},
    std::string_view{"org.tr"},
    std::string_view{"gov.tr"},
    std::string_view{"edu.tr"},

    std::string_view{"com.mx"},
    std::string_view{"org.mx"},
    std::string_view{"gob.mx"},

    std::string_view{"com.sg"},
    std::string_view{"com.hk"},
    std::string_view{"com.tw"},
    std::string_view{"com.ar"},
    std::string_view{"com.pl"},
    std::string_view{"com.ua"},
    std::string_view{"com.my"},
    std::string_view{"com.co"},

    // -- ICANN: wildcard registries and their exceptions ---------------------
    // These exist to prove the format works end to end; the full list has more.
    std::string_view{"*.ck"},
    std::string_view{"!www.ck"},
    std::string_view{"*.jm"},
    std::string_view{"*.kw"},

    // -- PRIVATE: hosting suffixes -------------------------------------------
    // Unrelated sites get subdomains here, so treating the parent as registrable
    // would let one project's cookies reach another's.
    std::string_view{"github.io"},
    std::string_view{"gitlab.io"},
    std::string_view{"pages.dev"},
    std::string_view{"workers.dev"},
    std::string_view{"netlify.app"},
    std::string_view{"vercel.app"},
    std::string_view{"herokuapp.com"},
    std::string_view{"appspot.com"},
    std::string_view{"web.app"},
    std::string_view{"firebaseapp.com"},
    std::string_view{"blogspot.com"},
    std::string_view{"azurewebsites.net"},
    std::string_view{"cloudfront.net"},
    std::string_view{"s3.amazonaws.com"},
    std::string_view{"glitch.me"},
    std::string_view{"wordpress.com"},
};

bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

// Mirrors Cookie.cpp's rule: IPv6 arrives bracketed or colon-bearing, IPv4 is
// digits and dots only. An address has no registry above it, so it has no
// public suffix either.
bool is_ip_literal(std::string_view host) {
    if (host.find(':') != std::string_view::npos) return true;
    return !host.empty() && std::all_of(host.begin(), host.end(), [](char c) { return is_digit(c) || c == '.'; }) &&
           std::any_of(host.begin(), host.end(), is_digit);
}

// Offsets at which each label of `host` starts, left to right. `count` labels.
struct Labels {
    static constexpr size_t kMax = 16;
    std::array<size_t, kMax> start{};
    size_t count = 0;

    std::string_view label(std::string_view host, size_t index) const {
        const size_t from = start[index];
        const size_t to = (index + 1 < count) ? start[index + 1] - 1 : host.size();
        return host.substr(from, to - from);
    }
    // The view covering the last `n` labels of `host`.
    std::string_view tail(std::string_view host, size_t n) const {
        if (n == 0 || n > count) return {};
        return host.substr(start[count - n]);
    }
};

// Splits on '.', stopping at kMax labels. A host deeper than that is not
// something the suffix table can classify, and the caller treats a zero count as
// "no suffix" rather than guessing.
Labels split_labels(std::string_view host) {
    Labels out;
    if (host.empty() || host.front() == '.' || host.back() == '.') return out;
    out.start[out.count++] = 0;
    for (size_t i = 0; i < host.size(); ++i) {
        if (host[i] != '.') continue;
        if (out.count >= Labels::kMax) return Labels{};
        if (i + 1 >= host.size()) return Labels{};  // trailing dot
        out.start[out.count++] = i + 1;
    }
    return out;
}

// Number of labels in a rule body, and whether its trailing labels match those
// of `host`. Returns 0 when it does not match.
size_t rule_match_length(std::string_view rule_body, std::string_view host, const Labels& labels) {
    const Labels rule_labels = split_labels(rule_body);
    if (rule_labels.count == 0 || rule_labels.count > labels.count) return 0;
    const size_t offset = labels.count - rule_labels.count;
    for (size_t i = 0; i < rule_labels.count; ++i) {
        const std::string_view expected = rule_labels.label(rule_body, i);
        if (expected == "*") continue;  // a wildcard matches exactly one label
        if (!Utils::equals_ignore_case(expected, labels.label(host, offset + i))) return 0;
    }
    return rule_labels.count;
}

}  // namespace

std::string_view public_suffix(std::string_view host) {
    if (host.empty() || is_ip_literal(host)) return {};
    const Labels labels = split_labels(host);
    if (labels.count == 0) return {};

    size_t best = 0;
    for (std::string_view rule : kRules) {
        const bool exception = !rule.empty() && rule.front() == '!';
        const std::string_view body = exception ? rule.substr(1) : rule;
        const size_t matched = rule_match_length(body, host, labels);
        if (matched == 0) continue;
        if (exception) {
            // An exception rule wins outright, and its suffix is the rule minus
            // its own leftmost label: `!www.ck` makes `ck` the suffix of
            // `www.ck`, so `www.ck` itself is registrable.
            return labels.tail(host, matched - 1);
        }
        best = std::max(best, matched);
    }
    // No rule matched: the PSL's implicit `*` default rule makes the TLD the
    // public suffix, which is right for every single-label registry.
    return labels.tail(host, best > 0 ? best : 1);
}

bool is_public_suffix(std::string_view host) {
    const std::string_view suffix = public_suffix(host);
    return !suffix.empty() && suffix.size() == host.size();
}

std::string_view registrable_domain(std::string_view host) {
    const std::string_view suffix = public_suffix(host);
    if (suffix.empty() || suffix.size() >= host.size()) {
        return {};  // an IP literal, or the host IS the public suffix
    }
    const Labels labels = split_labels(host);
    if (labels.count == 0) return {};
    // The suffix is a tail of `host` that begins at a label boundary, so its own
    // label count is just its dots plus one.
    const size_t suffix_labels = static_cast<size_t>(std::count(suffix.begin(), suffix.end(), '.')) + 1;
    if (suffix_labels >= labels.count) return {};
    return labels.tail(host, suffix_labels + 1);
}

}  // namespace Hummingbird::Core
