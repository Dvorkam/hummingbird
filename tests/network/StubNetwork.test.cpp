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
    EXPECT_NE(body.find("<link rel=\"stylesheet\" href=\"assets/stub.css\">"), std::string::npos);
    EXPECT_NE(body.find("<style>"), std::string::npos);
    EXPECT_NE(body.find("h1, h2, .title"), std::string::npos);
    EXPECT_NE(body.find("#lead"), std::string::npos);
    EXPECT_NE(body.find(".hidden { display: none; }"), std::string::npos);
    EXPECT_NE(body.find(".boxed { border-width: 20px; border-style: solid; border-color: #cc0000; padding: 4px; }"),
              std::string::npos);
    EXPECT_NE(body.find(".inline-block { display: inline-block; border-width: 1px; border-style: solid; border-color: #008000; padding: 2px; }"),
              std::string::npos);
    EXPECT_NE(body.find(".external-demo { color: #cc0000; }"), std::string::npos);
    EXPECT_NE(body.find("class=\"boxed\""), std::string::npos);
    EXPECT_NE(body.find("class=\"inline-block\""), std::string::npos);
    EXPECT_NE(body.find("class=\"external-demo\""), std::string::npos);
    EXPECT_NE(body.find("class=\"hidden\""), std::string::npos);
}
