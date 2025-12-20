#include <gtest/gtest.h>
#include "platform/StubNetwork.h"
#include <future>

TEST(StubNetworkTest, ReturnsExampleBody) {
    StubNetwork net;
    std::promise<std::string> p;
    auto fut = p.get_future();
    net.get("http://example.com", [&](std::string body) { p.set_value(body); });
    auto body = fut.get();
    EXPECT_NE(body.find("Example Domain"), std::string::npos);
}
