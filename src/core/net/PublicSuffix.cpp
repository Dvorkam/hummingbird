#include "core/net/PublicSuffix.h"

#include <algorithm>
#include <array>
#include <cstddef>

#include "core/net/PublicSuffixData.h"

namespace Hummingbird::Core {

namespace {

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

char to_lower_ascii(char c) {
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

// Three-way, ASCII-case-insensitive. Every generated rule is already lowercase,
// so folding both sides preserves the byte order the table is sorted in — which
// is what makes the binary search below valid.
int compare_ci(std::string_view a, std::string_view b) {
    const size_t shared = std::min(a.size(), b.size());
    for (size_t i = 0; i < shared; ++i) {
        const auto ca = static_cast<unsigned char>(to_lower_ascii(a[i]));
        const auto cb = static_cast<unsigned char>(to_lower_ascii(b[i]));
        if (ca != cb) return ca < cb ? -1 : 1;
    }
    if (a.size() == b.size()) return 0;
    return a.size() < b.size() ? -1 : 1;
}

template <size_t N>
bool contains(const std::array<std::string_view, N>& sorted, std::string_view needle) {
    if (needle.empty()) return false;
    const auto it =
        std::lower_bound(sorted.begin(), sorted.end(), needle,
                         [](std::string_view stored, std::string_view key) { return compare_ci(stored, key) < 0; });
    return it != sorted.end() && compare_ci(*it, needle) == 0;
}

// Offsets at which each label of `host` starts, left to right.
struct Labels {
    // DNS itself allows at most 127 labels, so a host past this is malformed
    // rather than merely deep.
    static constexpr size_t kMax = 128;
    std::array<size_t, kMax> start{};
    size_t count = 0;

    // The view covering the last `n` labels of `host`.
    std::string_view tail(std::string_view host, size_t n) const {
        if (n == 0 || n > count) return {};
        return host.substr(start[count - n]);
    }
    // `tail(host, n)` with its first label removed.
    std::string_view tail_parent(std::string_view host, size_t n) const {
        return n >= 2 ? tail(host, n - 1) : std::string_view{};
    }
};

// Splits on '.'. A leading dot, a trailing dot, an empty label, or more labels
// than DNS permits all yield a zero count, which every caller treats as "no
// suffix" rather than guessing.
Labels split_labels(std::string_view host) {
    Labels out;
    if (host.empty() || host.front() == '.' || host.back() == '.') return out;
    out.start[out.count++] = 0;
    for (size_t i = 0; i < host.size(); ++i) {
        if (host[i] != '.') continue;
        if (i + 1 >= host.size()) return Labels{};
        if (host[i + 1] == '.') return Labels{};  // empty label
        if (out.count >= Labels::kMax) return Labels{};
        out.start[out.count++] = i + 1;
    }
    return out;
}

}  // namespace

std::string_view public_suffix(std::string_view host) {
    if (host.empty() || is_ip_literal(host)) return {};
    const Labels labels = split_labels(host);
    if (labels.count == 0) return {};

    // publicsuffix.org's algorithm, in its stated order of precedence.
    //
    // Pass 1: an exception rule beats every other match regardless of length,
    // so it has to be looked for across all candidate suffixes before the
    // longest-match pass — not merely preferred at the level where it appears.
    // Its public suffix is the rule minus its own leftmost label: `!www.ck`
    // makes `ck` the suffix of `www.ck`, so `www.ck` itself is registrable.
    for (size_t n = labels.count; n >= 1; --n) {
        if (contains(PublicSuffixData::kExceptionRules, labels.tail(host, n))) {
            return labels.tail_parent(host, n);
        }
    }

    // Pass 2: longest match wins, so walk from the whole host downwards and stop
    // at the first hit. A wildcard rule is stored as its parent, so `*.ck`
    // matches `example.ck` when everything after the first label — `ck` — is in
    // the wildcard table.
    for (size_t n = labels.count; n >= 1; --n) {
        const std::string_view candidate = labels.tail(host, n);
        if (contains(PublicSuffixData::kExactRules, candidate)) return candidate;
        if (contains(PublicSuffixData::kWildcardParents, labels.tail_parent(host, n))) return candidate;
    }

    // No rule matched: the list's implicit `*` default rule makes the last label
    // the public suffix. With the full list bundled this means an unknown TLD.
    return labels.tail(host, 1);
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
