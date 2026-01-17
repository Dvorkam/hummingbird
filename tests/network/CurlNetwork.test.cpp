#include "platform/CurlNetwork.h"

#include <gtest/gtest.h>

TEST(CurlNetworkTest, AcceptEncodingIsEmptyForAutoDecompression) {
    EXPECT_STREQ(Hummingbird::Platform::CurlNetwork::accept_encoding(), "");
}
