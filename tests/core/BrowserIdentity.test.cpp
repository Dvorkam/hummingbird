#include "core/net/BrowserIdentity.h"

#include <gtest/gtest.h>

#include <optional>
#include <string>

using Hummingbird::Core::identity_headers;
using Hummingbird::Core::IdentityMode;

namespace {
std::optional<std::string> header_value(const std::vector<Hummingbird::Core::IdentityHeader>& headers,
                                        std::string_view name) {
    for (const auto& h : headers) {
        if (h.name == name) return h.value;
    }
    return std::nullopt;
}
}  // namespace

TEST(BrowserIdentityTest, TransparentUsesHonestUserAgent) {
    auto headers = identity_headers(IdentityMode::Transparent, /*secure=*/true);
    auto ua = header_value(headers, "User-Agent");
    ASSERT_TRUE(ua.has_value());
    EXPECT_NE(ua->find("Hummingbird/"), std::string::npos);  // version is not pinned here
    EXPECT_EQ(ua->find("Chrome/"), std::string::npos) << "Transparent must not claim Chrome";
}

TEST(BrowserIdentityTest, CompatibilityUsesCanonicalChromeUserAgent) {
    auto headers = identity_headers(IdentityMode::Compatibility, /*secure=*/true);
    auto ua = header_value(headers, "User-Agent");
    ASSERT_TRUE(ua.has_value());
    // Canonical: no embedded "Hummingbird" token in the UA string (that suffix is
    // exactly what HN's anti-crawler rule rejects).
    EXPECT_NE(ua->find("Chrome/120"), std::string::npos);
    EXPECT_NE(ua->find("Safari/537.36"), std::string::npos);
    EXPECT_EQ(ua->find("Hummingbird"), std::string::npos);
}

TEST(BrowserIdentityTest, SecChUaIsTruthfulAndIdenticalInBothModes) {
    auto transparent = identity_headers(IdentityMode::Transparent, true);
    auto compat = identity_headers(IdentityMode::Compatibility, true);
    // The identity carried in client hints is the same truthful value regardless
    // of the legacy UA-string mode.
    EXPECT_EQ(header_value(transparent, "Sec-CH-UA"), header_value(compat, "Sec-CH-UA"));
    auto sec = header_value(compat, "Sec-CH-UA");
    ASSERT_TRUE(sec.has_value());
    EXPECT_NE(sec->find("Hummingbird"), std::string::npos);
    EXPECT_EQ(sec->find("Chromium"), std::string::npos) << "we are not Chromium";
}

TEST(BrowserIdentityTest, ClientHintsAreSecureContextOnly) {
    auto insecure = identity_headers(IdentityMode::Compatibility, /*secure=*/false);
    EXPECT_FALSE(header_value(insecure, "Sec-CH-UA").has_value());
    EXPECT_FALSE(header_value(insecure, "Sec-CH-UA-Platform").has_value());
    // The User-Agent is always sent, secure or not.
    EXPECT_TRUE(header_value(insecure, "User-Agent").has_value());
}
