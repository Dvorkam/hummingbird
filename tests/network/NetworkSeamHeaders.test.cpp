// Story 8.1.0: request/response headers cross the network seam intact.
//
// The libcurl backend is not exercisable here (no network in the dev sandbox),
// so these lock the seam contract every backend implements and the cookie jar
// (8.1.1) depends on.
#include <gtest/gtest.h>

#include <string>
#include <utility>

#include "core/net/HttpHeaders.h"
#include "core/platform_api/INetwork.h"
#include "test_utils/TestFakes.h"

using Hummingbird::NetworkRequestOptions;
using Hummingbird::NetworkResponse;
using Hummingbird::Core::HttpHeaders;
using Hummingbird::Test::RoutingNetwork;

TEST(NetworkSeamHeadersTest, RequestHeadersReachTheBackend) {
    RoutingNetwork network;
    network.set_response("https://example.dev/", "<html></html>");

    NetworkRequestOptions options;
    options.headers.add("Cookie", "session=abc");

    network.get("https://example.dev/", [](NetworkResponse) {}, options);

    ASSERT_EQ(network.sent_headers().size(), 1u);
    EXPECT_EQ(network.sent_headers()[0].get("cookie"), "session=abc");
}

TEST(NetworkSeamHeadersTest, ResponseHeadersReachTheCaller) {
    RoutingNetwork network;
    network.set_response("https://example.dev/", "<html></html>");
    HttpHeaders served;
    served.add("Set-Cookie", "session=abc; Path=/; HttpOnly");
    served.add("Set-Cookie", "theme=dark");
    served.add("Content-Type", "text/html");
    network.set_response_headers("https://example.dev/", std::move(served));

    NetworkResponse received;
    network.get("https://example.dev/", [&](NetworkResponse response) { received = std::move(response); });

    // Both Set-Cookie fields survive as distinct values: a login response sets
    // several at once and collapsing them would drop all but one.
    const auto cookies = received.headers.get_all("Set-Cookie");
    ASSERT_EQ(cookies.size(), 2u);
    EXPECT_EQ(cookies[0], "session=abc; Path=/; HttpOnly");
    EXPECT_EQ(cookies[1], "theme=dark");
    EXPECT_EQ(received.headers.get("content-type"), "text/html");
}

TEST(NetworkSeamHeadersTest, PostCarriesRequestHeadersToo) {
    RoutingNetwork network;
    network.set_response("https://example.dev/login", "ok");

    NetworkRequestOptions options;
    options.headers.add("Cookie", "session=abc");

    network.post("https://example.dev/login", "user=x", [](NetworkResponse) {}, options);

    ASSERT_EQ(network.sent_headers().size(), 1u);
    EXPECT_EQ(network.sent_headers()[0].get("Cookie"), "session=abc");
}

TEST(NetworkSeamHeadersTest, ResponseWithoutHeadersIsEmptyNotMissing) {
    RoutingNetwork network;
    network.set_response("https://example.dev/", "<html></html>");

    NetworkResponse received;
    network.get("https://example.dev/", [&](NetworkResponse response) { received = std::move(response); });

    EXPECT_TRUE(received.headers.empty());
    EXPECT_TRUE(received.headers.get("Set-Cookie").empty());
}
