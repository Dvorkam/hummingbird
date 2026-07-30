// Story 9.4.1: static filter rulesets, declared in the manifest and read by the
// host. This is what gives "rules survive a restart" without any persistence:
// declared rules are simply read again next run.
#include "engine/extensions/FilterRuleSet.h"

#include <gtest/gtest.h>

#include <string>

using Hummingbird::Core::RequestDestination;
using Hummingbird::Core::ThirdPartyScope;
using Hummingbird::Engine::parse_filter_rule_set;

TEST(FilterRuleSetTest, ParsesAMinimalBlockRule) {
    const auto result = parse_filter_rule_set(R"([
      {"id": 7,
       "condition": {"requestDomains": ["doubleclick.net"]},
       "action": {"type": "block"}}
    ])");

    ASSERT_TRUE(result.ok()) << result.fatal_error;
    ASSERT_EQ(result.rules.size(), 1u);
    EXPECT_EQ(result.rules[0].id, 7u);
    EXPECT_EQ(result.rules[0].request_domain, "doubleclick.net");
    EXPECT_TRUE(result.rules[0].destinations.empty()) << "no resourceTypes means every filterable type";
    EXPECT_EQ(result.rules[0].third_party, ThirdPartyScope::Any);
    EXPECT_TRUE(result.warnings.empty());
}

TEST(FilterRuleSetTest, ParsesEverySupportedConditionField) {
    const auto result = parse_filter_rule_set(R"([
      {"id": 1,
       "condition": {"urlFilter": "/ads/",
                     "requestDomains": ["cdn.example.net"],
                     "resourceTypes": ["image", "script"],
                     "domainType": "thirdParty"},
       "action": {"type": "block"}}
    ])");

    ASSERT_TRUE(result.ok()) << result.fatal_error;
    ASSERT_EQ(result.rules.size(), 1u);
    const auto& rule = result.rules[0];
    EXPECT_EQ(rule.url_filter, "/ads/");
    EXPECT_EQ(rule.request_domain, "cdn.example.net");
    ASSERT_EQ(rule.destinations.size(), 2u);
    EXPECT_EQ(rule.destinations[0], RequestDestination::Image);
    EXPECT_EQ(rule.destinations[1], RequestDestination::Script);
    EXPECT_EQ(rule.third_party, ThirdPartyScope::ThirdPartyOnly);
}

TEST(FilterRuleSetTest, UnknownFieldsAreIgnoredSoAChromeRulesetStillLoads) {
    const auto result = parse_filter_rule_set(R"([
      {"id": 1, "priority": 2,
       "condition": {"urlFilter": "/ads/", "isUrlFilterCaseSensitive": false},
       "action": {"type": "block"}}
    ])");

    ASSERT_TRUE(result.ok()) << result.fatal_error;
    EXPECT_EQ(result.rules.size(), 1u);
}

// The dangerous rule. Accepting it would take the whole web off the air on
// behalf of an author who almost certainly made a typo.
TEST(FilterRuleSetTest, ARuleThatConstrainsNothingIsRejectedNotLoaded) {
    const auto result = parse_filter_rule_set(R"([
      {"id": 1, "condition": {}, "action": {"type": "block"}}
    ])");

    ASSERT_TRUE(result.ok());
    EXPECT_TRUE(result.rules.empty());
    ASSERT_EQ(result.warnings.size(), 1u);
    EXPECT_NE(result.warnings[0].find("matches everything"), std::string::npos);
}

// One typo in a fifty-rule list should cost that rule, not the list — but it
// must never be silent, because a rule that does not load is a rule that does
// not block and nothing outside says so.
TEST(FilterRuleSetTest, ABadRuleIsSkippedWithAWarningAndTheRestStillLoad) {
    const auto result = parse_filter_rule_set(R"([
      {"id": 1, "condition": {"urlFilter": "/good/"}, "action": {"type": "block"}},
      {"id": 2, "condition": {"urlFilter": "/x/", "resourceTypes": ["imagee"]}, "action": {"type": "block"}},
      {"id": 3, "condition": {"urlFilter": "/also-good/"}, "action": {"type": "block"}}
    ])");

    ASSERT_TRUE(result.ok()) << result.fatal_error;
    ASSERT_EQ(result.rules.size(), 2u);
    EXPECT_EQ(result.rules[0].id, 1u);
    EXPECT_EQ(result.rules[1].id, 3u);
    ASSERT_EQ(result.warnings.size(), 1u);
    EXPECT_NE(result.warnings[0].find("imagee"), std::string::npos) << "the warning must name the typo";
}

TEST(FilterRuleSetTest, ARuleWithNoActionIsRejected) {
    const auto result = parse_filter_rule_set(R"([
      {"id": 1, "condition": {"urlFilter": "/ads/"}}
    ])");

    ASSERT_TRUE(result.ok());
    EXPECT_TRUE(result.rules.empty());
    ASSERT_EQ(result.warnings.size(), 1u);
}

// `block` is the only action the engine can perform, so requiring the file to
// say so looks redundant. It is not: it means adding `allow` or `redirect`
// later cannot silently reinterpret every rule already written.
TEST(FilterRuleSetTest, AnUnsupportedActionIsRejectedRatherThanTreatedAsBlock) {
    const auto result = parse_filter_rule_set(R"([
      {"id": 1, "condition": {"urlFilter": "/ads/"}, "action": {"type": "allow"}}
    ])");

    ASSERT_TRUE(result.ok());
    EXPECT_TRUE(result.rules.empty()) << "an allow rule must not be loaded as a block rule";
    ASSERT_EQ(result.warnings.size(), 1u);
    EXPECT_NE(result.warnings[0].find("allow"), std::string::npos);
}

TEST(FilterRuleSetTest, MultipleRequestDomainsAreRejectedRatherThanTruncated) {
    const auto result = parse_filter_rule_set(R"([
      {"id": 1, "condition": {"requestDomains": ["a.net", "b.net"]}, "action": {"type": "block"}}
    ])");

    ASSERT_TRUE(result.ok());
    EXPECT_TRUE(result.rules.empty()) << "taking only the first domain would silently drop the rest";
    ASSERT_EQ(result.warnings.size(), 1u);
}

TEST(FilterRuleSetTest, AnEmptyRuleSetIsValid) {
    const auto result = parse_filter_rule_set("[]");
    EXPECT_TRUE(result.ok());
    EXPECT_TRUE(result.rules.empty());
    EXPECT_TRUE(result.warnings.empty());
}

TEST(FilterRuleSetTest, MalformedJsonFailsTheWholeFileAndLoadsNoRules) {
    for (const char* bad : {"", "{}", R"({"rules": []})", "[", R"([{"id": 1,])"}) {
        const auto result = parse_filter_rule_set(bad);
        EXPECT_FALSE(result.ok()) << "input: " << bad;
        EXPECT_TRUE(result.rules.empty())
            << "a file we could not read must contribute nothing, not a partial rule set: " << bad;
    }
}
