// Story 8.3.1: the engine owns the redirect loop, so hop limits, loop
// detection, per-hop cookies, and RFC 9110 method semantics are all testable
// without a real server.
#include <gtest/gtest.h>

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/net/CookieJar.h"
#include "engine/resources/RedirectPolicy.h"
#include "engine/resources/ResourceLoader.h"

namespace {
using Hummingbird::NetworkRequestOptions;
using Hummingbird::NetworkResponse;
using Hummingbird::Engine::ResourceLoader;
using Hummingbird::Engine::ResourceType;

// A fake server that answers with redirects until told otherwise, recording
// every request so a chain can be asserted hop by hop.
class RedirectingNetwork final : public Hummingbird::INetwork {
public:
    void redirect(const std::string& url, long status, std::string location) {
        hops_[url] = Hop{status, std::move(location)};
    }
    void set_body(const std::string& url, std::string body) { bodies_[url] = std::move(body); }
    void set_cookie(const std::string& url, std::string value) { cookies_[url] = std::move(value); }

    void get(const std::string& url, std::function<void(NetworkResponse)> callback,
             const NetworkRequestOptions& options = {}) override {
        answer(url, "GET", {}, options, std::move(callback));
    }
    void post(const std::string& url, std::string_view body, std::function<void(NetworkResponse)> callback,
              const NetworkRequestOptions& options = {}) override {
        answer(url, "POST", std::string(body), options, std::move(callback));
    }
    void shutdown() override {}

    struct Request {
        std::string url;
        std::string method;
        std::string body;
        NetworkRequestOptions options;
    };
    std::vector<Request> requests;

private:
    struct Hop {
        long status;
        std::string location;
    };

    void answer(const std::string& url, const char* method, std::string body, const NetworkRequestOptions& options,
                std::function<void(NetworkResponse)> callback) {
        requests.push_back(Request{url, method, std::move(body), options});
        NetworkResponse response;
        response.url = url;
        response.effective_url = url;
        if (auto cookie = cookies_.find(url); cookie != cookies_.end()) {
            response.headers.add("Set-Cookie", cookie->second);
        }
        if (auto hop = hops_.find(url); hop != hops_.end()) {
            response.status = hop->second.status;
            response.headers.add("Location", hop->second.location);
        } else {
            response.status = 200;
            if (auto found = bodies_.find(url); found != bodies_.end()) {
                response.body = found->second;
            }
        }
        if (callback) callback(std::move(response));
    }

    std::unordered_map<std::string, Hop> hops_;
    std::unordered_map<std::string, std::string> bodies_;
    std::unordered_map<std::string, std::string> cookies_;
};

// Answers every request with a redirect to a URL it has never used before, so
// only the hop cap can stop it.
class EndlessRedirectNetwork final : public Hummingbird::INetwork {
public:
    void get(const std::string& url, std::function<void(NetworkResponse)> callback,
             const NetworkRequestOptions& = {}) override {
        ++count;
        NetworkResponse response;
        response.url = url;
        response.effective_url = url;
        response.status = 302;
        response.headers.add("Location", "/hop" + std::to_string(count));
        if (callback) callback(std::move(response));
    }
    void post(const std::string& url, std::string_view, std::function<void(NetworkResponse)> callback,
              const NetworkRequestOptions& options = {}) override {
        get(url, std::move(callback), options);
    }
    void shutdown() override {}
    int count = 0;
};

ResourceLoader make_loader(std::unique_ptr<Hummingbird::INetwork> network,
                           std::shared_ptr<Hummingbird::Core::CookieJar> jar = nullptr) {
    return ResourceLoader(std::move(network), nullptr, nullptr, nullptr, std::move(jar));
}
}  // namespace

TEST(RedirectChainTest, FollowsAChainToItsDestinationAndReportsWhereItLanded) {
    auto network = std::make_unique<RedirectingNetwork>();
    auto* net = network.get();
    net->redirect("https://example.test/a", 302, "/b");
    net->redirect("https://example.test/b", 302, "/c");
    net->set_body("https://example.test/c", "<html>arrived</html>");

    ResourceLoader loader = make_loader(std::move(network));
    loader.navigate("https://example.test/a");
    auto batch = loader.consume_pending_updates();

    ASSERT_EQ(net->requests.size(), 3u);
    EXPECT_EQ(net->requests[1].url, "https://example.test/b");
    EXPECT_EQ(net->requests[2].url, "https://example.test/c");

    auto view = loader.view("https://example.test/a", ResourceType::Document);
    ASSERT_TRUE(view.has_value());
    EXPECT_EQ(view->body, "<html>arrived</html>");
    // The tab learns where it actually landed, so the URL bar and history can
    // show the destination rather than the URL that was asked for.
    EXPECT_EQ(batch.effective_url, "https://example.test/c");
}

TEST(RedirectChainTest, PostBecomesGetAcrossA302AndDropsItsBody) {
    auto network = std::make_unique<RedirectingNetwork>();
    auto* net = network.get();
    net->redirect("https://example.test/submit", 302, "/done");
    net->set_body("https://example.test/done", "ok");

    ResourceLoader loader = make_loader(std::move(network));
    ResourceLoader::DocumentRequest request{};
    request.method = ResourceLoader::DocumentRequest::Method::Post;
    request.body = "q=hello";
    request.content_type = "application/x-www-form-urlencoded";
    loader.navigate("https://example.test/submit", request);

    ASSERT_EQ(net->requests.size(), 2u);
    EXPECT_EQ(net->requests[0].method, "POST");
    EXPECT_EQ(net->requests[1].method, "GET");
    EXPECT_TRUE(net->requests[1].body.empty()) << "the body must go with the method";
    EXPECT_TRUE(net->requests[1].options.content_type.empty())
        << "a GET hop must not advertise a content type for a body it is not sending";
}

TEST(RedirectChainTest, PostAndBodySurviveA307) {
    auto network = std::make_unique<RedirectingNetwork>();
    auto* net = network.get();
    net->redirect("https://example.test/submit", 307, "/done");
    net->set_body("https://example.test/done", "ok");

    ResourceLoader loader = make_loader(std::move(network));
    ResourceLoader::DocumentRequest request{};
    request.method = ResourceLoader::DocumentRequest::Method::Post;
    request.body = "q=hello";
    loader.navigate("https://example.test/submit", request);

    ASSERT_EQ(net->requests.size(), 2u);
    EXPECT_EQ(net->requests[1].method, "POST");
    EXPECT_EQ(net->requests[1].body, "q=hello");
}

TEST(RedirectChainTest, ALoopTerminatesOnRevisitWithAClearError) {
    auto network = std::make_unique<RedirectingNetwork>();
    auto* net = network.get();
    net->redirect("https://example.test/a", 302, "/b");
    net->redirect("https://example.test/b", 302, "/a");

    ResourceLoader loader = make_loader(std::move(network));
    loader.navigate("https://example.test/a");

    // Caught on revisit rather than burning the whole hop budget.
    EXPECT_EQ(net->requests.size(), 2u);
    auto batch = loader.consume_pending_updates();
    EXPECT_EQ(batch.document_error, Hummingbird::NetworkError::RedirectLoop);
}

TEST(RedirectChainTest, AnEndlessChainStopsAtTheHopLimit) {
    auto network = std::make_unique<EndlessRedirectNetwork>();
    auto* net = network.get();

    ResourceLoader loader = make_loader(std::move(network));
    loader.navigate("https://example.test/start");

    EXPECT_EQ(net->count, Hummingbird::Engine::RedirectPolicy::kMaxHops + 1);
    auto batch = loader.consume_pending_updates();
    EXPECT_EQ(batch.document_error, Hummingbird::NetworkError::TooManyRedirects);
}

// 8.1.3's core case, already working because send_request owns both the jar and
// the redirect loop: a login POST that 302s with Set-Cookie lands authenticated.
TEST(RedirectChainTest, CookiesSetMidChainRideTheRemainingHops) {
    auto network = std::make_unique<RedirectingNetwork>();
    auto* net = network.get();
    net->redirect("https://example.test/login", 302, "/account");
    net->set_cookie("https://example.test/login", "session=abc; Path=/");
    net->set_body("https://example.test/account", "<html>welcome</html>");

    auto jar = std::make_shared<Hummingbird::Core::CookieJar>();
    ResourceLoader loader = make_loader(std::move(network), jar);
    ResourceLoader::DocumentRequest request{};
    request.method = ResourceLoader::DocumentRequest::Method::Post;
    request.body = "user=me&pw=secret";
    loader.navigate("https://example.test/login", request);

    ASSERT_EQ(net->requests.size(), 2u);
    EXPECT_TRUE(net->requests[0].options.headers.get("Cookie").empty());
    EXPECT_EQ(net->requests[1].options.headers.get("Cookie"), "session=abc")
        << "a cookie set by the 302 must be on the very next hop";
}

TEST(RedirectChainTest, CookiesAreRecomputedPerHopAndDoNotFollowCrossSite) {
    auto network = std::make_unique<RedirectingNetwork>();
    auto* net = network.get();
    net->redirect("https://example.test/out", 302, "https://other.test/landing");
    net->set_body("https://other.test/landing", "ok");

    auto jar = std::make_shared<Hummingbird::Core::CookieJar>();
    jar->store_from_header("https://example.test/", "session=abc", Hummingbird::Core::CookieClock::now());

    ResourceLoader loader = make_loader(std::move(network), jar);
    loader.navigate("https://example.test/out");

    ASSERT_EQ(net->requests.size(), 2u);
    EXPECT_EQ(net->requests[0].options.headers.get("Cookie"), "session=abc");
    EXPECT_TRUE(net->requests[1].options.headers.get("Cookie").empty())
        << "the cookie for example.test must not follow a redirect to other.test";
}

TEST(RedirectChainTest, ASubresourceRedirectIsFollowedToo) {
    auto network = std::make_unique<RedirectingNetwork>();
    auto* net = network.get();
    net->redirect("https://example.test/old.css", 301, "/new.css");
    net->set_body("https://example.test/new.css", "body{}");

    ResourceLoader loader = make_loader(std::move(network));
    loader.request_stylesheets({"https://example.test/old.css"}, "https://example.test/page");

    ASSERT_EQ(net->requests.size(), 2u);
    EXPECT_EQ(net->requests[1].url, "https://example.test/new.css");
}

// --- per-hop SameSite context (8.1.3) ----------------------------------------

TEST(RedirectChainTest, ACrossSiteHopDoesNotInheritSameSiteStanding) {
    // Address-bar navigation to example.test that bounces to other.test. The
    // first hop has no initiator (nothing is cross-site), but the second must
    // NOT inherit that standing, or a redirect would launder Strict cookies.
    auto network = std::make_unique<RedirectingNetwork>();
    auto* net = network.get();
    net->redirect("https://example.test/out", 302, "https://other.test/landing");
    net->set_body("https://other.test/landing", "ok");

    auto jar = std::make_shared<Hummingbird::Core::CookieJar>();
    const auto when = Hummingbird::Core::CookieClock::now();
    jar->store_from_header("https://other.test/", "strict=1; SameSite=Strict", when);
    jar->store_from_header("https://other.test/", "lax=2", when);

    ResourceLoader loader = make_loader(std::move(network), jar);
    loader.navigate("https://example.test/out");

    ASSERT_EQ(net->requests.size(), 2u);
    // Lax still rides: this is a top-level navigation with a safe method.
    EXPECT_EQ(net->requests[1].options.headers.get("Cookie"), "lax=2");
}

TEST(RedirectChainTest, ASameSiteHopKeepsItsStrictCookies) {
    auto network = std::make_unique<RedirectingNetwork>();
    auto* net = network.get();
    net->redirect("https://example.test/out", 302, "https://www.example.test/landing");
    net->set_body("https://www.example.test/landing", "ok");

    auto jar = std::make_shared<Hummingbird::Core::CookieJar>();
    jar->store_from_header("https://www.example.test/", "strict=1; SameSite=Strict; Domain=example.test",
                           Hummingbird::Core::CookieClock::now());

    ResourceLoader loader = make_loader(std::move(network), jar);
    loader.navigate("https://example.test/out");

    ASSERT_EQ(net->requests.size(), 2u);
    EXPECT_EQ(net->requests[1].options.headers.get("Cookie"), "strict=1")
        << "example.test -> www.example.test is same-site, so Strict survives";
}

TEST(RedirectChainTest, A302RewriteMakesTheNextHopSafeForLax) {
    // A cross-site POST carries no Lax cookies, but once a 302 rewrites it to a
    // GET the resulting top-level navigation does.
    auto network = std::make_unique<RedirectingNetwork>();
    auto* net = network.get();
    net->redirect("https://example.test/submit", 302, "https://other.test/done");
    net->set_body("https://other.test/done", "ok");

    auto jar = std::make_shared<Hummingbird::Core::CookieJar>();
    jar->store_from_header("https://other.test/", "lax=1", Hummingbird::Core::CookieClock::now());

    ResourceLoader loader = make_loader(std::move(network), jar);
    ResourceLoader::DocumentRequest request{};
    request.method = ResourceLoader::DocumentRequest::Method::Post;
    request.body = "x=1";
    request.initiator_host = "attacker.test";
    loader.navigate("https://example.test/submit", request);

    ASSERT_EQ(net->requests.size(), 2u);
    EXPECT_EQ(net->requests[1].method, "GET");
    EXPECT_EQ(net->requests[1].options.headers.get("Cookie"), "lax=1");
}

TEST(RedirectChainTest, A307PreservingPostKeepsTheNextHopUnsafeForLax) {
    // The mirror of the above: 307 keeps it a POST, so Lax stays withheld
    // cross-site. This is the CSRF path surviving a redirect.
    auto network = std::make_unique<RedirectingNetwork>();
    auto* net = network.get();
    net->redirect("https://example.test/submit", 307, "https://other.test/done");
    net->set_body("https://other.test/done", "ok");

    auto jar = std::make_shared<Hummingbird::Core::CookieJar>();
    jar->store_from_header("https://other.test/", "lax=1", Hummingbird::Core::CookieClock::now());

    ResourceLoader loader = make_loader(std::move(network), jar);
    ResourceLoader::DocumentRequest request{};
    request.method = ResourceLoader::DocumentRequest::Method::Post;
    request.body = "x=1";
    loader.navigate("https://example.test/submit", request);

    ASSERT_EQ(net->requests.size(), 2u);
    EXPECT_EQ(net->requests[1].method, "POST");
    EXPECT_TRUE(net->requests[1].options.headers.get("Cookie").empty())
        << "a cross-site POST must stay unauthenticated across a 307";
}
