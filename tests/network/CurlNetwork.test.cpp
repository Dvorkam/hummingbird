#include <gtest/gtest.h>

#include "platform/CurlNetwork.h"

TEST(CurlNetworkTest, AcceptEncodingIsEmptyForAutoDecompression) {
    EXPECT_STREQ(CurlNetwork::accept_encoding(), "");
}
