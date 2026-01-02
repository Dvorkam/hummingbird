#include "platform/CurlNetwork.h"

#include <gtest/gtest.h>

TEST(CurlNetworkTest, AcceptEncodingIsEmptyForAutoDecompression) {
    EXPECT_STREQ(CurlNetwork::accept_encoding(), "");
}
