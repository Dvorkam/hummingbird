#include <gtest/gtest.h>

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "core/net/PublicSuffix.h"
#include "core/net/PublicSuffixData.h"

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

// The engine has no IDNA layer, so a hostname typed in a non-Latin script
// reaches this code as UTF-8, not punycode. Both forms of an internationalized
// rule are bundled: matching only punycode would fall back to "the last label is
// the suffix" for those hosts, which is the too-permissive direction.
TEST(PublicSuffixTest, InternationalizedHostsMatchInEitherForm) {
    // 公司.cn is a registry in both encodings.
    EXPECT_TRUE(is_public_suffix("\xe5\x85\xac\xe5\x8f\xb8.cn"));
    EXPECT_TRUE(is_public_suffix("xn--55qx5d.cn"));

    // ...so two sites under it are not one site, whichever form they arrive in.
    EXPECT_EQ(registrable_domain("shishi.\xe5\x85\xac\xe5\x8f\xb8.cn"), "shishi.\xe5\x85\xac\xe5\x8f\xb8.cn");
    EXPECT_EQ(registrable_domain("www.shishi.xn--55qx5d.cn"), "shishi.xn--55qx5d.cn");
    EXPECT_NE(registrable_domain("a.\xe5\x85\xac\xe5\x8f\xb8.cn"), registrable_domain("b.\xe5\x85\xac\xe5\x8f\xb8.cn"));

    // A cookie may not be scoped to the registry itself.
    EXPECT_EQ(registrable_domain("\xe5\x85\xac\xe5\x8f\xb8.cn"), "");
}

// --- the list's own conformance vectors --------------------------------------

namespace {
struct SuffixVector {
    int line = 0;
    std::string host;
    std::string expected;  // empty means the vectors' "null"
};

// publicsuffix.org ships its own test file: one `host expected` pair per line,
// `null` for "no registrable domain", `//` comments. Vendored verbatim at the
// same upstream commit as the rule data, so the two cannot drift apart.
std::vector<SuffixVector> load_vectors() {
    std::ifstream file(std::string(HB_TEST_FIXTURE_DIR) + "/public_suffix_tests.txt", std::ios::binary);
    std::vector<SuffixVector> vectors;
    std::string line;
    int number = 0;
    while (std::getline(file, line)) {
        ++number;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line.rfind("//", 0) == 0) continue;
        std::istringstream fields(line);
        std::string host;
        std::string expected;
        if (!(fields >> host >> expected)) continue;
        // The vectors spell a null input as the literal word `null`.
        if (host == "null") host.clear();
        if (expected == "null") expected.clear();
        vectors.push_back({number, host, expected});
    }
    return vectors;
}
}  // namespace

TEST(PublicSuffixTest, MatchesTheUpstreamConformanceVectors) {
    const auto vectors = load_vectors();
    // A silently empty fixture would turn this into a test that always passes.
    ASSERT_GT(vectors.size(), 70u) << "public_suffix_tests.txt missing or truncated";

    // The vectors assume an API that canonicalizes its output to lowercase.
    // Ours returns a view INTO the caller's host, which cannot be lowercased
    // without allocating, and every caller compares case-insensitively anyway
    // (see MatchingIsCaseInsensitiveAndPreservesInputCase). Folding case here
    // compares the thing the vectors actually pin down: which labels form the
    // registrable domain.
    const auto fold = [](std::string text) {
        for (char& c : text) {
            if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
        }
        return text;
    };

    size_t failures = 0;
    for (const auto& vector : vectors) {
        const std::string actual = fold(std::string(registrable_domain(vector.host)));
        if (actual != fold(vector.expected)) {
            ++failures;
            ADD_FAILURE() << "line " << vector.line << ": registrable_domain(\"" << vector.host << "\") == \"" << actual
                          << "\", expected \"" << vector.expected << "\"";
        }
    }
    EXPECT_EQ(failures, 0u) << failures << " of " << vectors.size() << " upstream vectors failed";
}

// The rule tables and the vectors are vendored from one upstream commit. If a
// refresh updates one and not the other, the pairing is broken even when both
// files are individually valid.
TEST(PublicSuffixTest, VendoredDataRecordsItsUpstreamCommit) {
    EXPECT_EQ(Hummingbird::Core::PublicSuffixData::kUpstreamCommit.size(), 40u);
    EXPECT_GT(Hummingbird::Core::PublicSuffixData::kExactRules.size(), 5000u);
    EXPECT_FALSE(Hummingbird::Core::PublicSuffixData::kWildcardParents.empty());
    EXPECT_FALSE(Hummingbird::Core::PublicSuffixData::kExceptionRules.empty());
}
