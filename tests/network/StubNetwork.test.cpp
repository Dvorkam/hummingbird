#include "platform/StubNetwork.h"

#include <gtest/gtest.h>

#include <future>

using Hummingbird::NetworkResponse;

TEST(StubNetworkTest, ReturnsExampleBody) {
    Hummingbird::Platform::StubNetwork net;
    std::promise<std::string> p;
    auto fut = p.get_future();
    net.get("http://example.dev", [&](NetworkResponse response) { p.set_value(std::move(response.body)); });
    auto body = fut.get();
    EXPECT_NE(body.find("Hummingbird Feature Tour"), std::string::npos);
    EXPECT_NE(body.find("<link rel=\"stylesheet\" href=\"assets/stub.css\">"), std::string::npos);
    EXPECT_NE(body.find("<style>"), std::string::npos);
    EXPECT_NE(body.find("h1, h2, .title"), std::string::npos);
    EXPECT_NE(body.find("#lead"), std::string::npos);
    EXPECT_NE(body.find("Typography & Inline Elements"), std::string::npos);
    EXPECT_NE(body.find("Links & Navigation"), std::string::npos);
    EXPECT_NE(body.find("Lists"), std::string::npos);
    EXPECT_NE(body.find("Tables"), std::string::npos);
    EXPECT_NE(body.find("Images"), std::string::npos);
    EXPECT_NE(body.find("JavaScript Demo"), std::string::npos);
    EXPECT_NE(body.find(".hidden { display: none; }"), std::string::npos);
    EXPECT_NE(body.find(".boxed { border-width: 20px; border-style: solid; border-color: #cc0000; padding: 4px; }"),
              std::string::npos);
    EXPECT_NE(body.find(".inline-block { display: inline-block; border-width: 1px; border-style: solid; border-color: "
                        "#008000; padding: 2px; }"),
              std::string::npos);
    EXPECT_NE(body.find(".external-demo { color: #cc0000; }"), std::string::npos);
    EXPECT_NE(body.find("class=\"boxed\""), std::string::npos);
    EXPECT_NE(body.find("class=\"inline-block\""), std::string::npos);
    EXPECT_NE(body.find("class=\"external-demo\""), std::string::npos);
    EXPECT_NE(body.find("class=\"hidden\""), std::string::npos);
}
