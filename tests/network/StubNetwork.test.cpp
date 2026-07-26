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

    // The localStorage demo (8.2.2) is a static asset page.
    auto storage = fetch_body("https://example.dev/storage");
    EXPECT_NE(storage.find("localStorage"), std::string::npos);
    EXPECT_NE(storage.find("hb_loads"), std::string::npos);

    // The sessionStorage demo (8.2.3) is served the same way.
    auto session = fetch_body("https://example.dev/session");
    EXPECT_NE(session.find("sessionStorage"), std::string::npos);
    EXPECT_NE(session.find("hb_tab_loads"), std::string::npos);
}

TEST(StubNetworkTest, UnknownPageFallsBack) {
    auto body = fetch_body("https://example.dev/no-such-page");
    EXPECT_NE(body.find("Failed to load"), std::string::npos);
}

TEST(StubNetworkTest, NonDemoHostReturnsEmptySoTheErrorPageCanRender) {
    // The stub is the built-in demo backend; for a real host it has nothing, so it
    // returns an empty body and ResourceLoader renders the 8.3.2 error page instead
    // of masking the failure with a stub page.
    EXPECT_TRUE(fetch_body("https://news.ycombinator.com/").empty());
    EXPECT_TRUE(fetch_body("https://offline.invalid/").empty());
}

// --- cookie demo (8.1.1) -----------------------------------------------------
// The /cookies route is the one stub page that reads its request, so it can show
// the jar working through the real engine path rather than through a fake.

TEST(StubNetworkTest, CookieDemoEchoesTheCookieHeaderItReceived) {
    Hummingbird::Platform::StubNetwork net;
    Hummingbird::NetworkRequestOptions options;
    options.headers.add("Cookie", "hb_visits=4; hb_session=live");

    std::promise<NetworkResponse> p;
    auto fut = p.get_future();
    net.get("https://example.dev/cookies", [&](NetworkResponse r) { p.set_value(std::move(r)); }, options);
    NetworkResponse response = fut.get();

    EXPECT_NE(response.body.find("hb_visits=4; hb_session=live"), std::string::npos) << response.body;
}

TEST(StubNetworkTest, CookieDemoIncrementsTheVisitCounterItIsSent) {
    Hummingbird::Platform::StubNetwork net;
    Hummingbird::NetworkRequestOptions options;
    options.headers.add("Cookie", "hb_visits=4");

    std::promise<NetworkResponse> p;
    auto fut = p.get_future();
    net.get("https://example.dev/cookies", [&](NetworkResponse r) { p.set_value(std::move(r)); }, options);
    NetworkResponse response = fut.get();

    const auto set_cookies = response.headers.get_all("Set-Cookie");
    ASSERT_FALSE(set_cookies.empty());
    bool counter_advanced = false;
    bool session_cookie_present = false;
    for (std::string_view value : set_cookies) {
        if (value.find("hb_visits=5") != std::string_view::npos) counter_advanced = true;
        // A session cookie carries neither Max-Age nor Expires.
        if (value.find("hb_session=") != std::string_view::npos && value.find("Max-Age") == std::string_view::npos) {
            session_cookie_present = true;
        }
    }
    EXPECT_TRUE(counter_advanced);
    EXPECT_TRUE(session_cookie_present);
}

TEST(StubNetworkTest, CookieDemoStartsAtOneWithNoCookieHeader) {
    Hummingbird::Platform::StubNetwork net;
    std::promise<NetworkResponse> p;
    auto fut = p.get_future();
    net.get("https://example.dev/cookies", [&](NetworkResponse r) { p.set_value(std::move(r)); });
    NetworkResponse response = fut.get();

    bool starts_at_one = false;
    for (std::string_view value : response.headers.get_all("Set-Cookie")) {
        if (value.find("hb_visits=1") != std::string_view::npos) starts_at_one = true;
    }
    EXPECT_TRUE(starts_at_one);
}

TEST(StubNetworkTest, ScopedCookieRouteSetsAPathRestrictedCookie) {
    Hummingbird::Platform::StubNetwork net;
    std::promise<NetworkResponse> p;
    auto fut = p.get_future();
    net.get("https://example.dev/cookies/private", [&](NetworkResponse r) { p.set_value(std::move(r)); });
    NetworkResponse response = fut.get();

    const auto set_cookies = response.headers.get_all("Set-Cookie");
    ASSERT_EQ(set_cookies.size(), 1u);
    EXPECT_NE(set_cookies[0].find("Path=/cookies/private"), std::string_view::npos);
}

TEST(StubNetworkTest, CookieDemoShowsTheHttpOnlySideBySide) {
    // The page sets an HttpOnly cookie and asks document.cookie what it can see;
    // the demo is only meaningful if both halves are actually on the page.
    Hummingbird::Platform::StubNetwork net;
    std::promise<NetworkResponse> p;
    auto fut = p.get_future();
    net.get("https://example.dev/cookies", [&](NetworkResponse r) { p.set_value(std::move(r)); });
    NetworkResponse response = fut.get();

    bool sets_httponly = false;
    for (std::string_view value : response.headers.get_all("Set-Cookie")) {
        if (value.find("hb_secret") != std::string_view::npos &&
            value.find("HttpOnly") != std::string_view::npos) {
            sets_httponly = true;
        }
    }
    EXPECT_TRUE(sets_httponly);
    // The element the script fills in, and the script that reads document.cookie.
    EXPECT_NE(response.body.find("js-cookies"), std::string::npos);
    EXPECT_NE(response.body.find("document.cookie"), std::string::npos);
}
