#include <gtest/gtest.h>

#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <string_view>

#include "core/net/CookieJar.h"
#include "core/platform_api/INetwork.h"
#include "core/platform_api/ResourceProviderFactory.h"
#include "engine/forms/FormSubmission.h"
#include "engine/resources/ResourceStore.h"
#include "test_utils/HeadlessTabHarness.h"

// Story 8.4.1 (T-HN-E2E-1): the CI-runnable end-to-end session test. A fixture
// "server" (an in-process fake INetwork, the pattern of the other E2E harnesses)
// serves an HN-shaped flow: a login form, a credentialed POST that answers with a
// persistent Set-Cookie, an authenticated page gated on that cookie, and a logout
// that clears it. The test drives the real Tab pipeline — cookie jar, Set-Cookie
// capture, per-request Cookie attachment — and simulates a browser restart by
// persisting the jar to disk and reloading it into a fresh tab.
//
// The DOM form-field collection and the redirect+cookie landing have their own
// tests (FormSubmissionBuilder, RedirectChain); this proves the whole session
// chain holds together and fails CI if any link regresses.

namespace {

using Hummingbird::INetwork;
using Hummingbird::NetworkRequestOptions;
using Hummingbird::NetworkResponse;
using Hummingbird::Core::CookieClock;
using Hummingbird::Core::CookieJar;
using Hummingbird::Engine::FormSubmission;
using Hummingbird::Engine::FormSubmitMethod;
using Hummingbird::Engine::ResourceType;
using Hummingbird::Test::HeadlessTabHarness;

constexpr std::string_view kToken = "sess-abc123";
const std::string kHome = "https://fixture.test/";

std::string url_path(const std::string& url) {
    auto scheme = url.find("://");
    auto start = scheme == std::string::npos ? 0 : url.find('/', scheme + 3);
    if (start == std::string::npos) {
        return "/";
    }
    auto query = url.find('?', start);
    return url.substr(start, query == std::string::npos ? std::string::npos : query - start);
}

std::string logged_out_home() {
    return "<html><body><a id='login' href='/login'>login</a></body></html>";
}
std::string logged_in_home() {
    return "<html><body><a id='logout' href='/logout'>logout</a><p>welcome alice</p></body></html>";
}
std::string login_form() {
    return "<html><body><form action='/login' method='post'>"
           "<input name='acct' value='alice'>"
           "<input name='pw' type='password' value='hunter2'>"
           "<input type='submit' value='login'></form></body></html>";
}

// The fixture server. Authentication is entirely cookie-driven, exactly like the
// real thing: it reads the incoming Cookie header and answers accordingly.
class LoginFixtureNetwork : public INetwork {
public:
    // The Cookie header the server saw for each path, so a test can prove a
    // request actually carried the session.
    std::map<std::string, std::string> received_cookie;

    void get(const std::string& url, std::function<void(NetworkResponse)> callback,
             const NetworkRequestOptions& options) override {
        respond(url, /*is_post=*/false, {}, options, std::move(callback));
    }
    void post(const std::string& url, std::string_view body, std::function<void(NetworkResponse)> callback,
              const NetworkRequestOptions& options) override {
        respond(url, /*is_post=*/true, std::string(body), options, std::move(callback));
    }
    void shutdown() override {}

private:
    static bool has_valid_session(const NetworkRequestOptions& options) {
        const std::string_view cookie = options.headers.get("Cookie");
        return cookie.find("session=" + std::string(kToken)) != std::string_view::npos;
    }

    void respond(const std::string& url, bool is_post, const std::string& body, const NetworkRequestOptions& options,
                 std::function<void(NetworkResponse)> callback) {
        const std::string path = url_path(url);
        received_cookie[path] = std::string(options.headers.get("Cookie"));

        NetworkResponse response;
        response.url = url;
        response.effective_url = url;
        response.status = 200;
        const bool authed = has_valid_session(options);

        if (path == "/login" && is_post) {
            if (body.find("acct=alice") != std::string::npos && body.find("pw=hunter2") != std::string::npos) {
                // A persistent (Max-Age) session cookie, so it survives a restart —
                // the whole point of the North Star. HttpOnly, like a real session.
                response.headers.add("Set-Cookie",
                                     "session=" + std::string(kToken) + "; Path=/; Max-Age=86400; HttpOnly");
                response.body = logged_in_home();
            } else {
                response.body = login_form();  // bad credentials: back to the form
            }
        } else if (path == "/login") {
            response.body = login_form();
        } else if (path == "/comment" && is_post) {
            if (authed) {
                response.body = "<html><body>comment posted</body></html>";
            } else {
                response.status = 403;
                response.body = "<html><body>You have to be logged in to comment.</body></html>";
            }
        } else if (path == "/logout") {
            response.headers.add("Set-Cookie", "session=; Path=/; Max-Age=0");  // expire it
            response.body = logged_out_home();
        } else {
            response.body = authed ? logged_in_home() : logged_out_home();
        }

        if (callback) callback(std::move(response));
    }
};

std::unique_ptr<HeadlessTabHarness> make_harness(std::shared_ptr<CookieJar> jar, LoginFixtureNetwork** out = nullptr) {
    auto network = std::make_unique<LoginFixtureNetwork>();
    if (out) *out = network.get();
    return std::make_unique<HeadlessTabHarness>(std::move(network), /*fallback=*/nullptr,
                                                Hummingbird::create_resource_provider(), /*decoder=*/nullptr,
                                                /*script_engine=*/nullptr, std::move(jar), /*storage=*/nullptr);
}

bool document_shows(HeadlessTabHarness& harness, const std::string& url, std::string_view needle) {
    auto view = harness.resource_view(url, ResourceType::Document);
    return view && view->body.find(needle) != std::string_view::npos;
}

FormSubmission login_submission(const std::string& body) {
    FormSubmission submission;
    submission.url = "https://fixture.test/login";
    submission.method = FormSubmitMethod::Post;
    submission.body = body;
    submission.content_type = "application/x-www-form-urlencoded";
    return submission;
}

}  // namespace

TEST(LoginFlowHarnessTest, SessionChainAuthenticatesPersistsAndLogsOut) {
    namespace fs = std::filesystem;
    const auto jar_path = fs::temp_directory_path() / "hb_login_flow_test.tsv";
    fs::remove(jar_path);
    const auto now = CookieClock::now();

    auto jar = std::make_shared<CookieJar>();

    // --- session 1: start anonymous, then log in --------------------------------
    {
        LoginFixtureNetwork* net = nullptr;
        auto harness = make_harness(jar, &net);

        harness->navigate(kHome);
        harness->tick();
        EXPECT_TRUE(document_shows(*harness, kHome, "login")) << "should start logged out";
        EXPECT_FALSE(document_shows(*harness, kHome, "logout"));

        harness->tab().navigate(login_submission("acct=alice&pw=hunter2"));
        harness->tick();
        EXPECT_EQ(jar->size(), 1u) << "a successful login stores the session cookie";

        // A fresh request to the home page now carries the cookie and is served
        // the authenticated page.
        harness->navigate(kHome);
        harness->tick();
        EXPECT_TRUE(document_shows(*harness, kHome, "logout")) << "authenticated after login";
        EXPECT_NE(net->received_cookie["/"].find("session=" + std::string(kToken)), std::string::npos)
            << "the home request actually carried the session cookie";
    }

    // --- restart: persist the jar and reload it into a fresh process ------------
    EXPECT_EQ(jar->save_to(jar_path, now), 1u) << "the persistent session cookie is written";
    auto restarted_jar = std::make_shared<CookieJar>();
    EXPECT_EQ(restarted_jar->load_from(jar_path, now), 1u) << "and restored on restart";

    // --- session 2: still authenticated, then log out --------------------------
    {
        auto harness = make_harness(restarted_jar);

        harness->navigate(kHome);
        harness->tick();
        EXPECT_TRUE(document_shows(*harness, kHome, "logout")) << "session survives a restart";

        harness->navigate("https://fixture.test/logout");
        harness->tick();
        EXPECT_EQ(restarted_jar->size(), 0u) << "logout clears the session cookie";

        harness->navigate(kHome);
        harness->tick();
        EXPECT_TRUE(document_shows(*harness, kHome, "login")) << "logged out again after logout";
        EXPECT_FALSE(document_shows(*harness, kHome, "logout"));
    }

    fs::remove(jar_path);
}

TEST(LoginFlowHarnessTest, CommentPostIsRejectedWithoutASessionAndAcceptedWithOne) {
    auto jar = std::make_shared<CookieJar>();
    LoginFixtureNetwork* net = nullptr;
    auto harness = make_harness(jar, &net);

    // Posting a comment while logged out is refused by the server.
    FormSubmission comment;
    comment.url = "https://fixture.test/comment";
    comment.method = FormSubmitMethod::Post;
    comment.body = "text=hello";
    comment.content_type = "application/x-www-form-urlencoded";
    harness->tab().navigate(comment);
    harness->tick();
    EXPECT_TRUE(document_shows(*harness, "https://fixture.test/comment", "have to be logged in"));

    // Log in, then the same POST is accepted because it now carries the cookie.
    harness->tab().navigate(login_submission("acct=alice&pw=hunter2"));
    harness->tick();
    ASSERT_EQ(jar->size(), 1u);

    harness->tab().navigate(comment);
    harness->tick();
    EXPECT_TRUE(document_shows(*harness, "https://fixture.test/comment", "comment posted"));
    EXPECT_NE(net->received_cookie["/comment"].find("session=" + std::string(kToken)), std::string::npos)
        << "the authenticated comment POST carried the session cookie";
}
