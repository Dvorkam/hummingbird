#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <map>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <vector>

namespace Hummingbird::Core {

// What a request is FOR. The vocabulary a filter rule scopes itself by, modelled
// on MV3's `resourceTypes` (story 9.4.1).
//
// This is deliberately a core type rather than a reuse of `Engine::ResourceType`.
// The engine enum names slots in the engine's resource store; this one names a
// request's purpose, and the two only look alike. Keeping them separate is what
// stops `core/` from having to know what a resource store is — the loader maps
// between them at the single call site that has both in scope.
enum class RequestDestination : std::uint8_t {
    // A top-level navigation. Present so the vocabulary is complete; M9 never
    // matches it. Blocking a navigation needs a "blocked by extension"
    // interstitial or it renders as a network error, and that is a UX story of
    // its own — see milestone9.md, 9.4.1 reshape note 2.
    Document,
    Stylesheet,
    Image,
    Font,
    Script,
    // A script-initiated fetch()/XHR. MV3 calls this `xmlhttprequest`.
    Fetch,
    Count,  // sentinel — keep last
};

inline constexpr std::size_t kRequestDestinationCount = static_cast<std::size_t>(RequestDestination::Count);

// Parses the wire name used in a ruleset file ("image", "script", ...). Returns
// false for an unknown name, which the loader reports rather than ignores: a
// typo'd resource type in a filter list silently widens or narrows what gets
// blocked, and neither failure is visible from the outside.
bool parse_request_destination(std::string_view name, RequestDestination* out);

// Whether a request's target is a different site from the page that asked for
// it. "Site" is the registrable domain, not the host, so `img.example.com`
// loading from `example.com` is first-party.
enum class ThirdPartyScope : std::uint8_t {
    Any,
    ThirdPartyOnly,
    FirstPartyOnly,
};

// One block rule. Block-only by design: allow-list ("exception") rules are where
// real filter lists get complicated — EasyList's `@@` syntax is a large part of
// its total complexity — and nothing in M9 needs them. Deferred, not forgotten.
struct FilterRule {
    // Identifies the rule within its source, for logs and for deterministic
    // ordering. Not required to be unique; ties break on list position.
    std::uint32_t id = 0;

    // A substring the URL must contain, matched case-insensitively against the
    // full URL including scheme. MV3's `urlFilter` without the anchor syntax
    // (`|`, `||`, `^`) — those exist to make substring matching precise about
    // domain boundaries, which `request_domain` below does directly and more
    // legibly.
    std::string url_filter;

    // A host the request must target, matching that host and its subdomains:
    // "doubleclick.net" matches `doubleclick.net` and `ad.doubleclick.net` but
    // NOT `notdoubleclick.net`. Compared case-insensitively.
    std::string request_domain;

    // Which destinations this rule applies to. Empty means "every destination
    // this milestone matches" — which is every one except Document.
    std::vector<RequestDestination> destinations;

    ThirdPartyScope third_party = ThirdPartyScope::Any;

    // True when the rule can never match anything, because it constrains
    // nothing: a rule with no url_filter and no request_domain would block the
    // entire web. Such a rule is rejected at load rather than applied.
    bool constrains_something() const { return !url_filter.empty() || !request_domain.empty(); }
};

// Declarative request blocking, matched natively (story 9.4.1).
//
// Owned per profile and shared by every tab, exactly like `CookieJar`,
// `HttpCache` and `IdentityPolicyStore`; `ResourceLoader` consults it at the
// `send_request` choke point. Rules arrive from extensions, but nothing in this
// class knows that — which is the point: a user-level block list that is not an
// extension can populate it the same way.
//
// Self-synchronizing. Matching happens on whatever thread the transport answers
// on (a redirect hop is issued from a network callback), while rules are
// mutated from the main thread — so unlike the cookie jar, which the loader
// guards with its own mutex, this one carries its own lock. Read-mostly, hence
// the shared_mutex: many concurrent matches, rare writes.
class RequestFilter {
public:
    struct Request {
        std::string_view url;
        RequestDestination destination = RequestDestination::Fetch;
        // Host of the document that asked for this. Empty means user-initiated
        // (address bar, bookmark, history), which is treated as first-party —
        // the same convention `CookieRequestContext::initiator_host` uses.
        std::string_view initiator_host;
    };

    struct Match {
        bool blocked = false;
        // Which source and rule decided, so a blocked request can say WHY. An
        // ad-blocker that cannot explain itself is untriageable — "the page is
        // broken with the extension on" needs to name a rule.
        std::string source;
        std::uint32_t rule_id = 0;
    };

    // Replaces every rule contributed by `source` (an extension id). Replace
    // rather than append, so reloading or re-enabling an extension cannot leave
    // its previous rules behind — a stale block is invisible until something
    // mysteriously fails to load.
    void set_rules(std::string_view source, std::vector<FilterRule> rules);

    // Drops a source's rules entirely. Called when an extension is disabled:
    // per the M5 lifecycle rules a disabled extension must not still be acting
    // on the network.
    void remove_source(std::string_view source);

    void clear();

    // The verdict for `request`. Never blocks a Document destination (see the
    // enumerator's comment). A request with no matching rule, or an empty
    // filter, returns `blocked == false`.
    Match match(const Request& request) const;

    bool empty() const;
    std::size_t rule_count() const;
    std::size_t source_count() const;

    // How many requests this filter has blocked, for the 9.4.2 measurements.
    //
    // There is deliberately NO byte counter here. A blocked request was never
    // sent, so its size is unknown, and a "bytes avoided" number computed in
    // this class would be invented. Bytes are measured where they can be known:
    // by the mock server in the fixture test, and by a real before/after run
    // with the extension toggled.
    std::uint64_t blocked_count() const;
    void reset_counters();

private:
    // Sorted by source id, so iteration order — and therefore which rule is
    // reported for a URL several rules match — does not depend on the order
    // extensions happened to start in. Determinism here is what makes the
    // integration tests stable and a user's bug report reproducible.
    std::map<std::string, std::vector<FilterRule>, std::less<>> rules_by_source_;
    mutable std::shared_mutex mutex_;
    // Atomic, not merely guarded: `match` increments it while holding the SHARED
    // lock, which by definition several threads can hold at once. A plain
    // counter there would be a data race that only shows up under concurrent
    // subresource loads — i.e. exactly in the real browser and never in a test.
    mutable std::atomic<std::uint64_t> blocked_count_{0};
};

}  // namespace Hummingbird::Core
