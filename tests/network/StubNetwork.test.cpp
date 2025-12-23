#include <gtest/gtest.h>
#include "platform/StubNetwork.h"
#include <future>

TEST(StubNetworkTest, ReturnsExampleBody) {
    StubNetwork net;
    std::promise<std::string> p;
    auto fut = p.get_future();
    net.get("http://example.dev", [&](std::string body) { p.set_value(body); });
    auto body = fut.get();
    EXPECT_NE(body.find("Example Domain"), std::string::npos);
    EXPECT_NE(body.find("<style>"), std::string::npos);
    EXPECT_NE(body.find("h1, h2, .title"), std::string::npos);
    EXPECT_NE(body.find("#lead"), std::string::npos);
    EXPECT_NE(body.find("class=\"hidden\""), std::string::npos);
}
