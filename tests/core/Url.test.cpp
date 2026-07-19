#include "core/utils/Url.h"

#include <gtest/gtest.h>

TEST(UrlTest, NormalizeAddsHttpsForBareHost) {
    EXPECT_EQ(Hummingbird::Core::normalize_input_url("example.com"), "https://example.com");
    EXPECT_EQ(Hummingbird::Core::normalize_input_url("  example.com/path  "), "https://example.com/path");
}

TEST(UrlTest, NormalizeKeepsExistingScheme) {
    EXPECT_EQ(Hummingbird::Core::normalize_input_url("http://example.com"), "http://example.com");
    EXPECT_EQ(Hummingbird::Core::normalize_input_url("https://example.com"), "https://example.com");
}

TEST(UrlTest, NormalizeHandlesSchemeRelative) {
    EXPECT_EQ(Hummingbird::Core::normalize_input_url("//duckduckgo.com/"), "https://duckduckgo.com/");
}

TEST(UrlTest, ParseAbsoluteUrlSplitsParts) {
    auto parsed = Hummingbird::Core::parse_absolute_url("https://Example.com:8080/path");
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->scheme, "https");
    EXPECT_EQ(parsed->host, "example.com");
    ASSERT_TRUE(parsed->port.has_value());
    EXPECT_EQ(parsed->port.value(), 8080u);
    EXPECT_EQ(parsed->path, "/path");
}

TEST(UrlTest, ResolveUrlHandlesRelativePaths) {
    std::string_view base = "https://example.com/dir/page.html";
    EXPECT_EQ(Hummingbird::Core::resolve_url(base, "styles/main.css"), "https://example.com/dir/styles/main.css");
    EXPECT_EQ(Hummingbird::Core::resolve_url(base, "../img/logo.png"), "https://example.com/img/logo.png");
}

TEST(UrlTest, ResolveUrlHandlesSpecialForms) {
    std::string_view base = "https://example.com:8080/dir/page.html";
    EXPECT_EQ(Hummingbird::Core::resolve_url(base, "//cdn.example.com/a.css"), "https://cdn.example.com/a.css");
    EXPECT_EQ(Hummingbird::Core::resolve_url(base, "/styles/main.css"), "https://example.com:8080/styles/main.css");
    EXPECT_EQ(Hummingbird::Core::resolve_url(base, "/html/"), "https://example.com:8080/html/");
    EXPECT_EQ(Hummingbird::Core::resolve_url(base, "subdir/"), "https://example.com:8080/dir/subdir/");
    EXPECT_EQ(Hummingbird::Core::resolve_url(base, "?q=1"), "https://example.com:8080/dir/page.html?q=1");
    EXPECT_EQ(Hummingbird::Core::resolve_url(base, "#top"), "https://example.com:8080/dir/page.html#top");
}

TEST(UrlTest, ResolveUrlPreservesOpaquePseudoSchemes) {
    // Opaque pseudo-schemes carry no path to join against the base, so they are
    // returned verbatim instead of being mangled into "https://base/javascript:..."
    // (the Hacker News `[-]` toggle bug).
    std::string_view base = "https://news.ycombinator.com/item?id=1";
    EXPECT_EQ(Hummingbird::Core::resolve_url(base, "javascript:void(0)"), "javascript:void(0)");
    EXPECT_EQ(Hummingbird::Core::resolve_url(base, "JavaScript:foo()"), "JavaScript:foo()");
    EXPECT_EQ(Hummingbird::Core::resolve_url(base, "mailto:a@b.com"), "mailto:a@b.com");
    // A bare host:port href is ambiguous and left to the normal logic (unchanged).
    EXPECT_EQ(Hummingbird::Core::resolve_url(base, "vote?id=9"), "https://news.ycombinator.com/vote?id=9");
}

TEST(UrlTest, IsJavascriptUrl) {
    EXPECT_TRUE(Hummingbird::Core::is_javascript_url("javascript:void(0)"));
    EXPECT_TRUE(Hummingbird::Core::is_javascript_url("JavaScript:doThing()"));
    EXPECT_TRUE(Hummingbird::Core::is_javascript_url("  javascript:x"));  // leading space trimmed
    EXPECT_FALSE(Hummingbird::Core::is_javascript_url("https://example.com/javascript:x"));
    EXPECT_FALSE(Hummingbird::Core::is_javascript_url("mailto:a@b.com"));
    EXPECT_FALSE(Hummingbird::Core::is_javascript_url(""));
}
