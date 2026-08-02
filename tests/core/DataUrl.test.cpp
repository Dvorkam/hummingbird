// T-NET-DATA-URL-1. A `data:` URL carries its own bytes; the loader used to hand
// it to curl, so every inline SVG icon cost a failed request and rendered
// nothing. Found in a live browsing log:
//   [warn] [network] curl failed: url=data:image/svg+xml;charset=utf-8,%3Csvg...
#include "core/utils/DataUrl.h"

#include <gtest/gtest.h>

#include <string>

namespace {
using Hummingbird::Core::Utils::is_data_url;
using Hummingbird::Core::Utils::parse_data_url;
}  // namespace

TEST(DataUrlTest, RecognizesTheSchemeCaseInsensitivelyAndRejectsOthers) {
    EXPECT_TRUE(is_data_url("data:text/plain,x"));
    EXPECT_TRUE(is_data_url("DATA:text/plain,x"));
    EXPECT_TRUE(is_data_url("Data:,"));
    EXPECT_FALSE(is_data_url("https://example.test/a.svg"));
    EXPECT_FALSE(is_data_url("datafile:x"));
    EXPECT_FALSE(is_data_url(""));
}

// The exact shape from the log: percent-encoded SVG with a charset parameter.
TEST(DataUrlTest, DecodesThePercentEncodedSvgFromTheLiveLog) {
    const auto parsed = parse_data_url(
        "data:image/svg+xml;charset=utf-8,%3Csvg xmlns='http://www.w3.org/2000/svg' "
        "viewBox='0 0 280.27 594.55'%3E%3C/svg%3E");
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->mime_type, "image/svg+xml");
    EXPECT_EQ(parsed->charset, "utf-8");
    // Note the literal spaces and single quotes survived: a strict URL parser
    // would have rejected them, and rejecting the image is worse than accepting
    // the bytes the author plainly meant.
    EXPECT_EQ(parsed->data,
              "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 280.27 594.55'></svg>");
}

TEST(DataUrlTest, DecodesBase64) {
    const auto parsed = parse_data_url("data:text/plain;base64,SGVsbG8sIHdvcmxkIQ==");
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->mime_type, "text/plain");
    EXPECT_EQ(parsed->data, "Hello, world!");
}

// Real markup wraps long attribute values, so whitespace lands inside the
// payload; and hand-written URLs routinely omit padding.
TEST(DataUrlTest, ToleratesWhitespaceAndMissingBase64Padding) {
    const auto wrapped = parse_data_url("data:text/plain;base64,SGVsbG8s\n  IHdvcmxk\tIQ==");
    ASSERT_TRUE(wrapped.has_value());
    EXPECT_EQ(wrapped->data, "Hello, world!");

    const auto unpadded = parse_data_url("data:text/plain;base64,SGVsbG8sIHdvcmxkIQ");
    ASSERT_TRUE(unpadded.has_value());
    EXPECT_EQ(unpadded->data, "Hello, world!");

    // base64url, which appears in hand-written URLs; the bytes are the same.
    const auto url_alphabet = parse_data_url("data:application/octet-stream;base64,-_-_");
    ASSERT_TRUE(url_alphabet.has_value());
    EXPECT_EQ(url_alphabet->data.size(), 3u);
}

// Only the FIRST comma is the separator. An SVG path is nothing but commas, so
// splitting on the last one — or on all of them — would truncate every icon.
TEST(DataUrlTest, OnlyTheFirstCommaSeparatesMetadataFromPayload) {
    const auto parsed = parse_data_url("data:image/svg+xml,<path d='M0,0 L1,1 L2,2'/>");
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->mime_type, "image/svg+xml");
    EXPECT_EQ(parsed->data, "<path d='M0,0 L1,1 L2,2'/>");
}

// Per spec an omitted mediatype means text/plain;charset=US-ASCII.
TEST(DataUrlTest, DefaultsTheMediatypeWhenItIsOmitted) {
    const auto parsed = parse_data_url("data:,hello");
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->mime_type, "text/plain");
    EXPECT_EQ(parsed->charset, "us-ascii");
    EXPECT_EQ(parsed->data, "hello");

    const auto empty_payload = parse_data_url("data:,");
    ASSERT_TRUE(empty_payload.has_value());
    EXPECT_TRUE(empty_payload->data.empty()) << "an empty payload is valid, not a parse failure";
}

TEST(DataUrlTest, NormalizesCaseInTheMediatypeAndCharset) {
    const auto parsed = parse_data_url("data:IMAGE/SVG+XML;CHARSET=UTF-8,x");
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->mime_type, "image/svg+xml");
    EXPECT_EQ(parsed->charset, "utf-8");
}

TEST(DataUrlTest, RejectsUrlsThatCannotBeDecoded) {
    // No comma at all: there is no payload boundary, so this is not a data URL.
    EXPECT_FALSE(parse_data_url("data:image/png;base64").has_value());
    EXPECT_FALSE(parse_data_url("data:").has_value());
    // A character that cannot occur in base64 means the URL lied about itself.
    EXPECT_FALSE(parse_data_url("data:text/plain;base64,not valid ***").has_value());
    EXPECT_FALSE(parse_data_url("https://example.test/x").has_value());
}

// A malformed percent-escape is passed through rather than dropped: truncating
// at the bad byte would silently shorten an image, which is harder to notice
// than a stray '%'.
TEST(DataUrlTest, LeavesIncompletePercentEscapesAlone) {
    const auto parsed = parse_data_url("data:text/plain,100%25 done %zz %");
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->data, "100% done %zz %");
}
