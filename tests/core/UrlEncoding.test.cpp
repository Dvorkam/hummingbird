#include "core/utils/UrlEncoding.h"

#include <gtest/gtest.h>

TEST(UrlEncodingTest, UrlEncodeComponentUsesPercentForSpace) {
    EXPECT_EQ(Hummingbird::Core::Utils::url_encode_component("duck duck"), "duck%20duck");
}

TEST(UrlEncodingTest, FormUrlEncodeComponentUsesPlusForSpace) {
    EXPECT_EQ(Hummingbird::Core::Utils::form_url_encode_component("duck duck"), "duck+duck");
}

TEST(UrlEncodingTest, FormUrlEncodeComponentEscapesPlusSign) {
    EXPECT_EQ(Hummingbird::Core::Utils::form_url_encode_component("c++"), "c%2B%2B");
}
