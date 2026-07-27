#include <gtest/gtest.h>

#include "core/net/PublicSuffix.h"

namespace {
using Hummingbird::Core::is_public_suffix;
using Hummingbird::Core::public_suffix;
using Hummingbird::Core::registrable_domain;
}  // namespace

// Single-label registries need no table entry: the PSL's implicit `*` default
// rule already makes every TLD a public suffix.
TEST(PublicSuffixTest, SingleLabelTldsComeFromTheDefaultRule) {
    EXPECT_EQ(public_suffix("example.com"), "com");
    EXPECT_EQ(public_suffix("www.example.dev"), "dev");
    EXPECT_EQ(public_suffix("seznam.cz"), "cz");  // .cz has no second-level registry

    EXPECT_EQ(registrable_domain("example.com"), "example.com");
    EXPECT_EQ(registrable_domain("www.example.dev"), "example.dev");
    EXPECT_EQ(registrable_domain("a.b.c.example.com"), "example.com");
}

// The reason this story exists: "last two labels" called co.uk a registrable
// domain, so a.co.uk and b.co.uk looked like one site.
TEST(PublicSuffixTest, MultiLabelRegistriesGetOneMoreLabel) {
    EXPECT_EQ(public_suffix("example.co.uk"), "co.uk");
    EXPECT_EQ(public_suffix("www.example.co.uk"), "co.uk");
    EXPECT_EQ(registrable_domain("www.example.co.uk"), "example.co.uk");
    EXPECT_EQ(registrable_domain("a.co.uk"), "a.co.uk");
    EXPECT_NE(registrable_domain("a.co.uk"), registrable_domain("b.co.uk"));

    EXPECT_EQ(registrable_domain("shop.example.com.au"), "example.com.au");
    EXPECT_EQ(registrable_domain("news.example.co.jp"), "example.co.jp");
}

// Hosting suffixes are the private half: two unrelated projects share the parent
// domain, so the parent must not be registrable.
TEST(PublicSuffixTest, HostingSuffixesSeparateUnrelatedSites) {
    EXPECT_EQ(public_suffix("alice.github.io"), "github.io");
    EXPECT_EQ(registrable_domain("alice.github.io"), "alice.github.io");
    EXPECT_NE(registrable_domain("alice.github.io"), registrable_domain("mallory.github.io"));
    EXPECT_EQ(registrable_domain("app.herokuapp.com"), "app.herokuapp.com");
}

// A host that IS a public suffix has nothing registrable under it.
TEST(PublicSuffixTest, PublicSuffixItselfHasNoRegistrableDomain) {
    EXPECT_TRUE(is_public_suffix("com"));
    EXPECT_TRUE(is_public_suffix("co.uk"));
    EXPECT_TRUE(is_public_suffix("github.io"));
    EXPECT_FALSE(is_public_suffix("example.com"));
    EXPECT_FALSE(is_public_suffix("example.co.uk"));

    EXPECT_EQ(registrable_domain("com"), "");
    EXPECT_EQ(registrable_domain("co.uk"), "");
}

// Wildcard and exception rules, exercised end to end so the table can carry the
// real PSL syntax when the full list lands.
TEST(PublicSuffixTest, WildcardAndExceptionRulesFollowThePslAlgorithm) {
    // `*.ck` makes every second-level label a suffix...
    EXPECT_EQ(public_suffix("example.ck"), "example.ck");
    EXPECT_EQ(registrable_domain("site.example.ck"), "site.example.ck");
    // ...except `!www.ck`, which pulls the suffix back to `ck`.
    EXPECT_EQ(public_suffix("www.ck"), "ck");
    EXPECT_EQ(registrable_domain("www.ck"), "www.ck");

    // `*.sch.uk`: a school's own label is the suffix, so two schools are not
    // one site even though both sit under sch.uk.
    EXPECT_EQ(public_suffix("pupil.faringdon.sch.uk"), "faringdon.sch.uk");
    EXPECT_NE(registrable_domain("a.one.sch.uk"), registrable_domain("a.two.sch.uk"));
}

// An address has no registry above it, and a bare label has nothing below its.
TEST(PublicSuffixTest, IpLiteralsAndDegenerateHostsHaveNoSuffix) {
    EXPECT_EQ(public_suffix("127.0.0.1"), "");
    EXPECT_EQ(registrable_domain("127.0.0.1"), "");
    EXPECT_EQ(public_suffix("[::1]"), "");
    EXPECT_EQ(registrable_domain("::1"), "");
    EXPECT_EQ(public_suffix(""), "");
    EXPECT_EQ(registrable_domain("localhost"), "");  // "localhost" is its own suffix
    EXPECT_EQ(public_suffix("example.com."), "");    // trailing dot is not canonical here
}

TEST(PublicSuffixTest, MatchingIsCaseInsensitiveAndPreservesInputCase) {
    EXPECT_EQ(public_suffix("WWW.Example.CO.UK"), "CO.UK");
    EXPECT_EQ(registrable_domain("WWW.Example.CO.UK"), "Example.CO.UK");
    EXPECT_TRUE(is_public_suffix("CO.UK"));
}
