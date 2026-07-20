#include "platform/net/StubNetwork.h"

#include <gtest/gtest.h>

#include <future>
#include <string>

using Hummingbird::NetworkResponse;

namespace {

std::string fetch_body(const std::string& url) {
    Hummingbird::Platform::StubNetwork net;
    std::promise<std::string> p;
    auto fut = p.get_future();
    net.get(url, [&](NetworkResponse response) { p.set_value(std::move(response.body)); });
    return fut.get();
}

}  // namespace

TEST(StubNetworkTest, ReturnsExampleBody) {
    auto body = fetch_body("http://example.dev");
    // Landing page: hero, flex layout, and links to every per-milestone demo page.
    EXPECT_NE(body.find("Hummingbird Browser Engine"), std::string::npos);
    EXPECT_NE(body.find("<style>"), std::string::npos);
    EXPECT_NE(body.find("display: flex"), std::string::npos);
    EXPECT_NE(body.find("class=\"hero\""), std::string::npos);
    EXPECT_NE(body.find("https://example.dev/m1"), std::string::npos);
    EXPECT_NE(body.find("https://example.dev/m2"), std::string::npos);
    EXPECT_NE(body.find("https://example.dev/m3"), std::string::npos);
    EXPECT_NE(body.find("https://example.dev/m4"), std::string::npos);
    EXPECT_NE(body.find("https://example.dev/m5"), std::string::npos);
    EXPECT_NE(body.find("https://example.dev/m6"), std::string::npos);
    EXPECT_NE(body.find("https://example.dev/m8"), std::string::npos);
    EXPECT_NE(body.find("https://example.dev/search"), std::string::npos);
}

TEST(StubNetworkTest, ServesMilestoneSubpages) {
    auto m6 = fetch_body("https://example.dev/m6");
    EXPECT_NE(m6.find("The Layouter"), std::string::npos);
    EXPECT_NE(m6.find("justify-content"), std::string::npos);

    auto m4 = fetch_body("https://example.dev/m4");
    EXPECT_NE(m4.find("The Brain"), std::string::npos);
    EXPECT_NE(m4.find("js-demo"), std::string::npos);

    auto js = fetch_body("https://example.dev/js");
    EXPECT_NE(js.find("JS Quick Check"), std::string::npos);
}

TEST(StubNetworkTest, UnknownPageFallsBack) {
    auto body = fetch_body("https://example.dev/no-such-page");
    EXPECT_NE(body.find("Failed to load"), std::string::npos);
}
