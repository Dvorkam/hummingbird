#include "core/net/RequestFilter.h"

#include <gtest/gtest.h>

#include <atomic>
#include <cstdint>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

using Hummingbird::Core::FilterRule;
using Hummingbird::Core::parse_request_destination;
using Hummingbird::Core::RequestDestination;
using Hummingbird::Core::RequestFilter;
using Hummingbird::Core::ThirdPartyScope;

namespace {

FilterRule url_rule(std::string filter, std::uint32_t id = 1) {
    FilterRule rule;
    rule.id = id;
    rule.url_filter = std::move(filter);
    return rule;
}

FilterRule domain_rule(std::string domain, std::uint32_t id = 1) {
    FilterRule rule;
    rule.id = id;
    rule.request_domain = std::move(domain);
    return rule;
}

RequestFilter::Request request(std::string_view url, RequestDestination destination = RequestDestination::Script,
                               std::string_view initiator = "example.com") {
    return RequestFilter::Request{url, destination, initiator};
}

}  // namespace

TEST(RequestFilterTest, AnEmptyFilterBlocksNothing) {
    RequestFilter filter;
    EXPECT_TRUE(filter.empty());
    EXPECT_FALSE(filter.match(request("https://ads.example.net/track.js")).blocked);
    EXPECT_EQ(filter.blocked_count(), 0u);
}

TEST(RequestFilterTest, ASubstringRuleMatchesAnywhereInTheUrlAndIgnoresCase) {
    RequestFilter filter;
    filter.set_rules("ad-block-lite", {url_rule("/ads/")});

    EXPECT_TRUE(filter.match(request("https://cdn.example.net/ads/banner.js")).blocked);
    EXPECT_TRUE(filter.match(request("https://cdn.example.net/ADS/banner.js")).blocked)
        << "a list author writing /ads/ does not expect /ADS/ to slip past";
    EXPECT_TRUE(filter.match(request("https://cdn.example.net/x?src=/ads/")).blocked) << "the query counts too";
    EXPECT_FALSE(filter.match(request("https://cdn.example.net/adsense/banner.js")).blocked)
        << "/ads/ requires the trailing slash it was written with";
}

// The dot boundary. A plain suffix test is how a naive blocker takes down an
// unrelated site whose domain merely ends with a blocked one.
TEST(RequestFilterTest, ADomainRuleMatchesSubdomainsButNotSuffixLookalikes) {
    RequestFilter filter;
    filter.set_rules("ad-block-lite", {domain_rule("doubleclick.net")});

    EXPECT_TRUE(filter.match(request("https://doubleclick.net/pixel")).blocked);
    EXPECT_TRUE(filter.match(request("https://ad.g.doubleclick.net/pixel")).blocked);
    EXPECT_FALSE(filter.match(request("https://notdoubleclick.net/pixel")).blocked);
    EXPECT_FALSE(filter.match(request("https://doubleclick.net.evil.com/pixel")).blocked)
        << "the blocked domain must be the END of the host, not merely present in it";
}

TEST(RequestFilterTest, TheHostIsReadPastUserinfoAndPort) {
    RequestFilter filter;
    filter.set_rules("s", {domain_rule("tracker.net")});

    EXPECT_TRUE(filter.match(request("https://tracker.net:8443/p")).blocked);
    EXPECT_TRUE(filter.match(request("https://user:pw@tracker.net/p")).blocked);
    // The classic spoof: the real host is `evil.com`, with the blocked domain
    // hidden in the userinfo where a careless parser would find it.
    EXPECT_FALSE(filter.match(request("https://tracker.net@evil.com/p")).blocked)
        << "userinfo is not the host; reading it as one inverts the verdict";
}

TEST(RequestFilterTest, BothConstraintsMustHoldWhenBothAreGiven) {
    RequestFilter filter;
    FilterRule rule;
    rule.request_domain = "cdn.example.net";
    rule.url_filter = "/track";
    filter.set_rules("s", {rule});

    EXPECT_TRUE(filter.match(request("https://cdn.example.net/track/p.gif")).blocked);
    EXPECT_FALSE(filter.match(request("https://cdn.example.net/app.js")).blocked) << "domain alone is not enough";
    EXPECT_FALSE(filter.match(request("https://other.net/track/p.gif")).blocked) << "substring alone is not enough";
}

TEST(RequestFilterTest, ARuleThatConstrainsNothingIsInert) {
    RequestFilter filter;
    // No url_filter and no request_domain: this would otherwise block the web.
    filter.set_rules("s", {FilterRule{}});

    EXPECT_FALSE(filter.match(request("https://example.com/app.js")).blocked);
    EXPECT_EQ(filter.blocked_count(), 0u);
}

TEST(RequestFilterTest, ResourceTypeScopesARule) {
    RequestFilter filter;
    FilterRule rule = domain_rule("tracker.net");
    rule.destinations = {RequestDestination::Image};
    filter.set_rules("s", {rule});

    EXPECT_TRUE(filter.match(request("https://tracker.net/p.gif", RequestDestination::Image)).blocked);
    EXPECT_FALSE(filter.match(request("https://tracker.net/p.js", RequestDestination::Script)).blocked);
}

TEST(RequestFilterTest, AnEmptyResourceTypeListMatchesEveryDestinationExceptDocument) {
    RequestFilter filter;
    filter.set_rules("s", {domain_rule("tracker.net")});

    for (const auto destination : {RequestDestination::Stylesheet, RequestDestination::Image,
                                   RequestDestination::Font, RequestDestination::Script, RequestDestination::Fetch}) {
        EXPECT_TRUE(filter.match(request("https://tracker.net/x", destination)).blocked);
    }
}

// M9 scope limit, asserted rather than merely documented: blocking a navigation
// needs an interstitial that does not exist, so a rule that would match one must
// not fire. If this ever starts blocking, a user gets a blank tab with no
// explanation.
TEST(RequestFilterTest, ATopLevelNavigationIsNeverBlocked) {
    RequestFilter filter;
    FilterRule rule = domain_rule("tracker.net");
    rule.destinations = {RequestDestination::Document};
    filter.set_rules("s", {rule});

    EXPECT_FALSE(filter.match(request("https://tracker.net/page", RequestDestination::Document)).blocked);
    EXPECT_EQ(filter.blocked_count(), 0u) << "a refused-by-scope request is not a block, and must not be counted";
}

TEST(RequestFilterTest, ThirdPartyScopeComparesSitesNotHosts) {
    RequestFilter filter;
    FilterRule rule = url_rule("/beacon");
    rule.third_party = ThirdPartyScope::ThirdPartyOnly;
    filter.set_rules("s", {rule});

    EXPECT_TRUE(filter.match(request("https://tracker.net/beacon", RequestDestination::Image, "example.com")).blocked);
    EXPECT_FALSE(filter.match(request("https://example.com/beacon", RequestDestination::Image, "example.com")).blocked);
    EXPECT_FALSE(filter.match(request("https://img.example.com/beacon", RequestDestination::Image, "www.example.com"))
                     .blocked)
        << "a subdomain of the same registrable domain is first-party";
}

TEST(RequestFilterTest, AUserInitiatedRequestCountsAsFirstParty) {
    RequestFilter filter;
    FilterRule third = url_rule("/beacon", 1);
    third.third_party = ThirdPartyScope::ThirdPartyOnly;
    FilterRule first = url_rule("/beacon", 2);
    first.third_party = ThirdPartyScope::FirstPartyOnly;

    filter.set_rules("s", {third});
    EXPECT_FALSE(filter.match(request("https://tracker.net/beacon", RequestDestination::Image, "")).blocked);
    filter.set_rules("s", {first});
    EXPECT_TRUE(filter.match(request("https://tracker.net/beacon", RequestDestination::Image, "")).blocked);
}

// Determinism: which rule is reported must not depend on which extension
// happened to start first, or a bug report naming a rule id is unreproducible.
TEST(RequestFilterTest, TheReportedRuleDoesNotDependOnRegistrationOrder) {
    const auto reported = [](bool zebra_first) {
        RequestFilter filter;
        if (zebra_first) {
            filter.set_rules("zebra", {url_rule("/track", 20)});
            filter.set_rules("alpha", {url_rule("/track", 10)});
        } else {
            filter.set_rules("alpha", {url_rule("/track", 10)});
            filter.set_rules("zebra", {url_rule("/track", 20)});
        }
        return filter.match(request("https://x.net/track"));
    };

    const auto a = reported(true);
    const auto b = reported(false);
    ASSERT_TRUE(a.blocked);
    ASSERT_TRUE(b.blocked);
    EXPECT_EQ(a.source, b.source);
    EXPECT_EQ(a.rule_id, b.rule_id);
    EXPECT_EQ(a.source, "alpha") << "sources are consulted in sorted order, not insertion order";
}

TEST(RequestFilterTest, AMatchNamesTheSourceAndRuleThatDecided) {
    RequestFilter filter;
    filter.set_rules("ad-block-lite", {url_rule("/ads/", 42)});

    const auto match = filter.match(request("https://x.net/ads/a.js"));
    ASSERT_TRUE(match.blocked);
    EXPECT_EQ(match.source, "ad-block-lite");
    EXPECT_EQ(match.rule_id, 42u) << "a block that cannot name its rule is untriageable";
}

// A stale rule is invisible until something mysteriously fails to load, so
// re-registering must REPLACE rather than accumulate.
TEST(RequestFilterTest, SettingRulesReplacesThatSourcesPreviousRules) {
    RequestFilter filter;
    filter.set_rules("s", {url_rule("/old")});
    ASSERT_TRUE(filter.match(request("https://x.net/old")).blocked);

    filter.set_rules("s", {url_rule("/new")});
    EXPECT_FALSE(filter.match(request("https://x.net/old")).blocked) << "the superseded rule must be gone";
    EXPECT_TRUE(filter.match(request("https://x.net/new")).blocked);
    EXPECT_EQ(filter.rule_count(), 1u);
}

// Per the M5 lifecycle rules a disabled extension must not still be acting on
// the network.
TEST(RequestFilterTest, RemovingASourceStopsItsRulesFromActing) {
    RequestFilter filter;
    filter.set_rules("keep", {url_rule("/keep")});
    filter.set_rules("drop", {url_rule("/drop")});
    ASSERT_EQ(filter.source_count(), 2u);

    filter.remove_source("drop");
    EXPECT_EQ(filter.source_count(), 1u);
    EXPECT_FALSE(filter.match(request("https://x.net/drop")).blocked);
    EXPECT_TRUE(filter.match(request("https://x.net/keep")).blocked);

    filter.clear();
    EXPECT_TRUE(filter.empty());
    EXPECT_FALSE(filter.match(request("https://x.net/keep")).blocked);
}

TEST(RequestFilterTest, SettingAnEmptyRuleListDropsTheSource) {
    RequestFilter filter;
    filter.set_rules("s", {url_rule("/x")});
    filter.set_rules("s", {});
    EXPECT_TRUE(filter.empty()) << "an extension that clears its rules must leave nothing behind";
}

TEST(RequestFilterTest, AUrlWithNoHostIsNeverBlocked) {
    RequestFilter filter;
    filter.set_rules("s", {url_rule("track")});

    // data: and about: have no host to attribute a rule to. They must fall
    // through rather than be matched on their payload, which for a data: URL is
    // attacker-supplied text that could contain any substring at all.
    EXPECT_FALSE(filter.match(request("data:text/html,<p>track</p>")).blocked);
    EXPECT_FALSE(filter.match(request("about:blank")).blocked);
    EXPECT_FALSE(filter.match(request("")).blocked);
}

TEST(RequestFilterTest, BlockedRequestsAreCounted) {
    RequestFilter filter;
    filter.set_rules("s", {domain_rule("tracker.net")});

    EXPECT_TRUE(filter.match(request("https://tracker.net/a")).blocked);
    EXPECT_TRUE(filter.match(request("https://tracker.net/b")).blocked);
    EXPECT_FALSE(filter.match(request("https://example.com/c")).blocked);
    EXPECT_EQ(filter.blocked_count(), 2u) << "only blocks count, not every request examined";

    filter.reset_counters();
    EXPECT_EQ(filter.blocked_count(), 0u);
}

TEST(RequestFilterTest, DestinationNamesParseFromTheirMv3SpellingAndRejectTypos) {
    RequestDestination destination = RequestDestination::Count;
    EXPECT_TRUE(parse_request_destination("image", &destination));
    EXPECT_EQ(destination, RequestDestination::Image);
    EXPECT_TRUE(parse_request_destination("xmlhttprequest", &destination));
    EXPECT_EQ(destination, RequestDestination::Fetch);
    EXPECT_TRUE(parse_request_destination("main_frame", &destination));
    EXPECT_EQ(destination, RequestDestination::Document);

    // A typo must be reported, never ignored: silently dropping an unknown
    // resource type widens the rule to every type, which is the opposite of
    // what the author asked for.
    EXPECT_FALSE(parse_request_destination("imagee", &destination));
    EXPECT_FALSE(parse_request_destination("fetch", &destination));
    EXPECT_FALSE(parse_request_destination("", &destination));
}

// Rules are mutated from the main thread (an extension registering them) while
// requests are matched on transport threads (a redirect hop is issued from a
// network callback). This is the arrangement the class carries its own lock
// for, so it is worth actually running.
TEST(RequestFilterTest, MatchingWhileRulesChangeIsSafe) {
    RequestFilter filter;
    filter.set_rules("s", {domain_rule("tracker.net")});

    std::atomic<bool> stop{false};
    std::vector<std::thread> readers;
    for (int i = 0; i < 4; ++i) {
        readers.emplace_back([&] {
            while (!stop.load()) {
                (void)filter.match(request("https://tracker.net/p.gif", RequestDestination::Image));
                (void)filter.match(request("https://example.com/app.js"));
            }
        });
    }
    for (int i = 0; i < 200; ++i) {
        filter.set_rules("s", {domain_rule("tracker.net", static_cast<std::uint32_t>(i))});
        filter.set_rules("other", {url_rule("/beacon")});
        filter.remove_source("other");
    }
    stop.store(true);
    for (auto& reader : readers) reader.join();

    EXPECT_TRUE(filter.match(request("https://tracker.net/p.gif", RequestDestination::Image)).blocked);
}
