#include "core/net/HttpHeaders.h"

#include <gtest/gtest.h>

using Hummingbird::Core::HttpHeaders;

TEST(HttpHeadersTest, LookupIsCaseInsensitiveButStorageKeepsTheSentCasing) {
    HttpHeaders headers;
    headers.add("Content-Type", "text/html");

    EXPECT_EQ(headers.get("content-type"), "text/html");
    EXPECT_EQ(headers.get("CONTENT-TYPE"), "text/html");
    EXPECT_TRUE(headers.contains("Content-Type"));
    ASSERT_EQ(headers.fields().size(), 1u);
    EXPECT_EQ(headers.fields()[0].name, "Content-Type");
}

TEST(HttpHeadersTest, MissingHeaderReadsEmpty) {
    HttpHeaders headers;
    EXPECT_TRUE(headers.get("Set-Cookie").empty());
    EXPECT_FALSE(headers.contains("Set-Cookie"));
    EXPECT_TRUE(headers.get_all("Set-Cookie").empty());
}

// The reason this is a list and not a map: a login response sets several cookies
// in separate Set-Cookie fields, and collapsing them loses all but one.
TEST(HttpHeadersTest, RepeatedFieldsAreKeptSeparatelyInReceiptOrder) {
    HttpHeaders headers;
    headers.add("Set-Cookie", "session=abc; Path=/");
    headers.add("Set-Cookie", "theme=dark");

    const auto cookies = headers.get_all("set-cookie");
    ASSERT_EQ(cookies.size(), 2u);
    EXPECT_EQ(cookies[0], "session=abc; Path=/");
    EXPECT_EQ(cookies[1], "theme=dark");
    // get() still yields the first, for genuinely single-valued fields.
    EXPECT_EQ(headers.get("Set-Cookie"), "session=abc; Path=/");
}

TEST(HttpHeadersTest, SetReplacesEveryExistingOccurrence) {
    HttpHeaders headers;
    headers.add("X-Trace", "one");
    headers.add("x-trace", "two");
    headers.add("Accept", "text/html");

    headers.set("X-Trace", "three");

    EXPECT_EQ(headers.get_all("x-trace").size(), 1u);
    EXPECT_EQ(headers.get("X-Trace"), "three");
    EXPECT_EQ(headers.get("Accept"), "text/html");  // untouched
}

TEST(HttpHeadersTest, RawLineParsingTrimsAndSplitsOnTheFirstColon) {
    HttpHeaders headers;
    ASSERT_TRUE(headers.add_raw_line("Location: https://example.dev/a?x=1\r\n"));

    EXPECT_EQ(headers.get("location"), "https://example.dev/a?x=1");
}

// curl hands the header callback the status line and the blank separator too, so
// add_raw_line has to reject anything that is not a field.
TEST(HttpHeadersTest, RawLineParsingSkipsNonFieldLines) {
    HttpHeaders headers;
    EXPECT_FALSE(headers.add_raw_line("HTTP/1.1 200 OK\r\n"));
    EXPECT_FALSE(headers.add_raw_line("\r\n"));
    EXPECT_FALSE(headers.add_raw_line(""));
    EXPECT_FALSE(headers.add_raw_line(": novalue\r\n"));
    EXPECT_TRUE(headers.empty());
}

TEST(HttpHeadersTest, RawLineParsingAcceptsAnEmptyValue) {
    HttpHeaders headers;
    ASSERT_TRUE(headers.add_raw_line("X-Empty:\r\n"));
    EXPECT_TRUE(headers.contains("X-Empty"));
    EXPECT_TRUE(headers.get("X-Empty").empty());
}
