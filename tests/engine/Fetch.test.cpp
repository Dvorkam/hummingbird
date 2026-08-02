// Story 9.1.1: fetch() end to end, through every layer it actually crosses —
// QuickJS binding -> IScriptHost -> DocumentScriptHost -> Tab -> ResourceLoader
// -> INetwork, and back the other way on the main thread.
//
// The shape being proven is that a fetch returns a Promise NOW and settles it
// LATER: nothing in this file settles a promise from the call that started it.
#include <gtest/gtest.h>

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/ArenaAllocator.h"
#include "core/dom/Element.h"
#include "core/platform_api/ResourceProviderFactory.h"
#include "core/platform_api/ScriptEngineFactory.h"
#include "core/platform_api/ScriptFetch.h"
#include "engine/resources/ResourceLoader.h"
#include "engine/script/DocumentScriptHost.h"
#include "test_utils/HeadlessTabHarness.h"
#include "test_utils/TestFakes.h"

namespace {
using Hummingbird::ScriptFetchFailure;
using Hummingbird::ScriptFetchRequest;
using Hummingbird::ScriptFetchResponse;

// Stands in for the Tab: takes requests, hands back ids, and lets the test
// decide when (and whether) each one answers — which is the only way to prove
// the promise really is pending in between.
class FetchDriver {
public:
    void install(Hummingbird::Engine::DocumentScriptHost& host) {
        host.set_url_resolver([](std::string_view relative) {
            std::string url(relative);
            return url.rfind("http", 0) == 0 ? url : "https://example.test" + url;
        });
        host.set_fetch_sink([this](const ScriptFetchRequest& request) -> std::uint64_t {
            requests.push_back(request);
            return ++next_id_;
        });
    }

    // Answers request `id` as a server would, on the main thread — the Tab's
    // tick is what does this for real.
    ScriptFetchResponse make_response(std::uint64_t id, long status, std::string body) {
        ScriptFetchResponse response;
        response.id = id;
        response.status = status;
        response.url = requests.at(id - 1).url;
        response.body = std::move(body);
        response.headers.add("Content-Type", "application/json");
        return response;
    }

    std::vector<ScriptFetchRequest> requests;

private:
    std::uint64_t next_id_ = 0;
};

struct FetchFixture {
    Hummingbird::Core::ArenaAllocator arena{8192, 8};
    Hummingbird::Engine::DocumentScriptHost host;
    Hummingbird::ScriptEnginePtr engine;
    Hummingbird::Core::ArenaPtr<Hummingbird::DOM::Element> root;
    Hummingbird::DOM::Element* out = nullptr;
    FetchDriver driver;

    FetchFixture() {
        root = Hummingbird::DOM::Element::create(arena, "div");
        auto out_el = Hummingbird::DOM::Element::create(arena, "div");
        out_el->set_attribute("id", "out");
        out_el->set_attribute("data-log", "");
        out = out_el.get();
        root->append_child(std::move(out_el));
        host.reset(root.get(), &arena);
        driver.install(host);
        engine = Hummingbird::create_script_engine();
        engine->bind_host(&host);
    }
    std::string log() { return host.get_attribute(out, "data-log"); }
    // The script preamble every case uses: append to the DOM, which is the one
    // channel visible from C++.
    static std::string logger() {
        return "function log(c){var o=document.getElementById('out');"
               "  o.setAttribute('data-log', o.getAttribute('data-log') + c);}";
    }
};
}  // namespace

// The core contract: fetch returns immediately with nothing decided, and the
// continuation runs only once the response is settled in.
TEST(FetchTest, ResolvesWithTheResponseBodyOnlyAfterItIsSettled) {
    FetchFixture fx;
    auto result = fx.engine->eval(FetchFixture::logger() +
                                      "fetch('/api/news').then(function (response) {"
                                      "  log('status:' + response.status + ' ok:' + response.ok + ' ');"
                                      "  return response.text();"
                                      "}).then(function (text) { log('body:' + text); });",
                                  "inline");
    ASSERT_TRUE(result.ok) << result.error;

    // The request left, but nothing has resolved: the promise is genuinely
    // pending, not synchronously completed behind the scenes.
    ASSERT_EQ(fx.driver.requests.size(), 1u);
    EXPECT_EQ(fx.driver.requests[0].url, "https://example.test/api/news");
    EXPECT_EQ(fx.driver.requests[0].method, "GET");
    EXPECT_EQ(fx.log(), "");

    EXPECT_TRUE(fx.engine->settle_fetch(fx.driver.make_response(1, 200, "hello")));
    EXPECT_EQ(fx.log(), "status:200 ok:true body:hello");
}

// Per the Fetch standard only a NETWORK error rejects. A 404 is a perfectly good
// response with ok == false, and a page that treats it as a throw breaks against
// a real server.
TEST(FetchTest, HttpErrorStatusResolvesRatherThanRejecting) {
    FetchFixture fx;
    ASSERT_TRUE(fx.engine
                    ->eval(FetchFixture::logger() +
                               "fetch('/missing').then(function (r) { log('resolved:' + r.status + ':' + r.ok); },"
                               "                       function () { log('rejected'); });",
                           "inline")
                    .ok);

    fx.engine->settle_fetch(fx.driver.make_response(1, 404, "nope"));
    EXPECT_EQ(fx.log(), "resolved:404:false");
}

// A transport failure DOES reject, and a timeout is distinguishable from every
// other failure — the JS-visible half of story 9.1.3.
TEST(FetchTest, NetworkFailureRejectsAndATimeoutIsNamed) {
    FetchFixture fx;
    ASSERT_TRUE(fx.engine
                    ->eval(FetchFixture::logger() + "fetch('/slow').then(function () { log('resolved'); },"
                                                    "                    function (e) { log('rejected:' + e.name); });",
                           "inline")
                    .ok);

    ScriptFetchResponse timed_out;
    timed_out.id = 1;
    timed_out.failure = ScriptFetchFailure::Timeout;
    fx.engine->settle_fetch(timed_out);
    EXPECT_EQ(fx.log(), "rejected:TimeoutError");

    ASSERT_TRUE(fx.engine
                    ->eval("fetch('/gone').then(function () { log('resolved'); },"
                           "                    function (e) { log(' then:' + e.name); });",
                           "inline")
                    .ok);
    ScriptFetchResponse failed;
    failed.id = 2;
    failed.failure = ScriptFetchFailure::NetworkError;
    fx.engine->settle_fetch(failed);
    EXPECT_EQ(fx.log(), "rejected:TimeoutError then:TypeError");
}

TEST(FetchTest, SendsMethodBodyAndHeaders) {
    FetchFixture fx;
    ASSERT_TRUE(fx.engine
                    ->eval("fetch('/submit', { method: 'post', credentials: 'include',"
                           "  headers: { 'X-Test': 'yes', 'Content-Type': 'application/json' },"
                           "  body: '{\"a\":1}' });",
                           "inline")
                    .ok);

    ASSERT_EQ(fx.driver.requests.size(), 1u);
    const auto& request = fx.driver.requests[0];
    EXPECT_EQ(request.method, "POST");  // normalized to upper case
    EXPECT_TRUE(request.has_body);
    EXPECT_EQ(request.body, "{\"a\":1}");
    EXPECT_EQ(request.headers.get("X-Test"), "yes");
    EXPECT_EQ(request.headers.get("Content-Type"), "application/json");
    EXPECT_EQ(request.credentials, Hummingbird::Core::Cors::Credentials::Include);
}

TEST(FetchTest, JsonParsesAndHeadersAreCaseInsensitive) {
    FetchFixture fx;
    ASSERT_TRUE(fx.engine
                    ->eval(FetchFixture::logger() + "fetch('/api').then(function (r) {"
                                                    "  log('ct:' + r.headers.get('content-TYPE') + ' ');"
                                                    "  return r.json();"
                                                    "}).then(function (data) { log('title:' + data[0].title); });",
                           "inline")
                    .ok);

    fx.engine->settle_fetch(fx.driver.make_response(1, 200, "[{\"title\":\"Hello\"}]"));
    EXPECT_EQ(fx.log(), "ct:application/json title:Hello");
}

// The spec makes a body single-use. Enforcing it catches the "read it twice and
// silently get nothing" bug at the point of the mistake.
TEST(FetchTest, ABodyMayOnlyBeReadOnce) {
    FetchFixture fx;
    ASSERT_TRUE(fx.engine
                    ->eval(FetchFixture::logger() + "fetch('/api').then(function (r) {"
                                                    "  return r.text().then(function () { return r.text(); });"
                                                    "}).then(function () { log('read-twice'); },"
                                                    "        function (e) { log('refused:' + e.name); });",
                           "inline")
                    .ok);

    fx.engine->settle_fetch(fx.driver.make_response(1, 200, "once"));
    EXPECT_EQ(fx.log(), "refused:TypeError");
}

// THE lifetime guarantee, and the reason 9.0.2 had to land first: a response
// that arrives after the document is gone must not run page A's continuation.
TEST(FetchTest, TeardownCancelsInFlightFetchesWithoutFiringCallbacks) {
    FetchFixture fx;
    ASSERT_TRUE(fx.engine
                    ->eval(FetchFixture::logger() + "fetch('/slow').then(function () { log('resolved'); },"
                                                    "                    function () { log('rejected'); });",
                           "inline")
                    .ok);
    EXPECT_EQ(fx.engine->pending_fetch_count(), 1u);

    // --- navigation ---
    fx.engine->reset_bindings();
    EXPECT_EQ(fx.engine->pending_fetch_count(), 0u);

    // The response now shows up late. It must find nothing to settle...
    EXPECT_FALSE(fx.engine->settle_fetch(fx.driver.make_response(1, 200, "too late")));
    // ...and neither continuation may have run: not the success path, and not
    // the failure path either. Silence is the correct outcome, because the page
    // that would have observed either one no longer exists.
    EXPECT_EQ(fx.log(), "");
}

// Regression: a fetch to the built-in demo site must take the SAME route the
// page around it took. example.dev is served by the stub network and has no DNS
// behind it, so sending a fetch to the real transport made it fail to resolve
// while the document that issued it had loaded fine. Every fetch test above
// stubs the sink, so none of them could see this — it needs the real loader.
TEST(FetchTest, ADemoSiteFetchUsesTheSameTransportAsTheDocument) {
    // Distinguishable transports: the "real" one records and fails like DNS
    // would, the "fallback" one answers.
    class RecordingNetwork final : public Hummingbird::INetwork {
    public:
        explicit RecordingNetwork(std::string body) : body_(std::move(body)) {}
        void get(const std::string& url, std::function<void(Hummingbird::NetworkResponse)> callback,
                 const Hummingbird::NetworkRequestOptions& = {}) override {
            urls.push_back(url);
            Hummingbird::NetworkResponse response;
            response.url = url;
            response.effective_url = url;
            if (body_.empty()) {
                response.error = Hummingbird::NetworkError::CurlError;  // as an unresolvable host does
            } else {
                response.status = 200;
                response.body = body_;
            }
            if (callback) callback(std::move(response));
        }
        void post(const std::string& url, std::string_view, std::function<void(Hummingbird::NetworkResponse)> callback,
                  const Hummingbird::NetworkRequestOptions& options = {}) override {
            get(url, std::move(callback), options);
        }
        void shutdown() override {}
        std::vector<std::string> urls;

    private:
        std::string body_;
    };

    auto real = std::make_unique<RecordingNetwork>("");              // no DNS for example.dev
    auto stub = std::make_unique<RecordingNetwork>("[{\"id\":1}]");  // the demo site
    auto* real_raw = real.get();
    auto* stub_raw = stub.get();

    Hummingbird::Engine::ResourceLoader loader(std::move(real), std::move(stub), nullptr, nullptr, nullptr);

    ScriptFetchRequest request;
    request.url = "https://example.dev/api/news";
    ScriptFetchResponse got;
    bool answered = false;
    loader.fetch_for_script(request, "https://example.dev/m9", [&](ScriptFetchResponse response) {
        got = std::move(response);
        answered = true;
    });

    ASSERT_TRUE(answered);
    EXPECT_TRUE(real_raw->urls.empty()) << "a demo-site fetch must not go to the real network";
    ASSERT_EQ(stub_raw->urls.size(), 1u);
    EXPECT_EQ(stub_raw->urls[0], "https://example.dev/api/news");
    EXPECT_EQ(got.failure, ScriptFetchFailure::None);
    EXPECT_EQ(got.status, 200);
    EXPECT_EQ(got.body, "[{\"id\":1}]");
}

// A fetch to a real host still goes to the real transport — the routing above
// must not swallow everything.
TEST(FetchTest, ANonDemoFetchStillUsesTheRealTransport) {
    class CountingNetwork final : public Hummingbird::INetwork {
    public:
        void get(const std::string& url, std::function<void(Hummingbird::NetworkResponse)> callback,
                 const Hummingbird::NetworkRequestOptions& = {}) override {
            urls.push_back(url);
            Hummingbird::NetworkResponse response;
            response.url = url;
            response.effective_url = url;
            response.status = 200;
            response.body = "live";
            // This test is about which transport ran, not about CORS — but the
            // target IS cross-origin from the document, so without the server's
            // opt-in 9.2.1 would (correctly) discard the body and the routing
            // assertion would fail for an unrelated reason.
            response.headers.set("Access-Control-Allow-Origin", "*");
            if (callback) callback(std::move(response));
        }
        void post(const std::string& url, std::string_view, std::function<void(Hummingbird::NetworkResponse)> callback,
                  const Hummingbird::NetworkRequestOptions& options = {}) override {
            get(url, std::move(callback), options);
        }
        void shutdown() override {}
        std::vector<std::string> urls;
    };

    auto real = std::make_unique<CountingNetwork>();
    auto stub = std::make_unique<CountingNetwork>();
    auto* real_raw = real.get();
    auto* stub_raw = stub.get();
    Hummingbird::Engine::ResourceLoader loader(std::move(real), std::move(stub), nullptr, nullptr, nullptr);

    ScriptFetchRequest request;
    request.url = "https://api.hnpwa.com/v0/news/1.json";
    ScriptFetchResponse got;
    loader.fetch_for_script(request, "https://example.test/page",
                            [&](ScriptFetchResponse response) { got = std::move(response); });

    EXPECT_EQ(real_raw->urls.size(), 1u);
    EXPECT_TRUE(stub_raw->urls.empty());
    EXPECT_EQ(got.body, "live");
}

// The whole loop through a real Tab: click -> fetch -> settle on tick -> the
// PAINTED output changes.
//
// Every other test here stops at "the DOM was mutated". That is not the same
// claim: settling only marks the tab dirty, and a repaint draws the render tree
// built BEFORE the continuation ran. The page's data changed and the screen did
// not — which is exactly what a fetch demo looks like when it is broken, and
// what no test above could see.
TEST(FetchTest, ASettledFetchRebuildsWhatIsPainted) {
    auto network = std::make_unique<Hummingbird::Test::RoutingNetwork>();
    auto* net = network.get();
    net->set_response("https://example.test/page", R"HTML(
<!doctype html>
<html>
  <head><style> body { margin: 0; padding: 0; } p { display: block; } </style></head>
  <body>
    <p id="out">before</p>
    <script>
      fetch('/data.json').then(function (r) { return r.json(); }).then(function (data) {
        document.getElementById('out').textContent = data.headline;
      });
    </script>
  </body>
</html>
)HTML");
    net->set_response("https://example.test/data.json", "{\"headline\":\"fetched-and-painted\"}");

    Hummingbird::Test::HeadlessTabHarness harness(std::move(network), nullptr, Hummingbird::create_resource_provider());
    harness.navigate("https://example.test/page");
    harness.tick();  // document ready: scripts run, the fetch goes out

    const auto painted = [&]() {
        harness.context().drawn_texts.clear();
        harness.paint();
        return harness.context().drawn_texts;
    };
    const auto shows = [&](const char* text) {
        const auto texts = painted();
        return std::find(texts.begin(), texts.end(), text) != texts.end();
    };

    // A second tick drains the settled response and must rebuild before painting.
    harness.tick();
    EXPECT_TRUE(shows("fetched-and-painted")) << "the fetch settled but the painted tree was not rebuilt";
    EXPECT_FALSE(shows("before"));
}

// --- CORS enforcement through the real loader (story 9.2.1) ------------------

namespace {
// A server the test writes response headers for, recording exactly what was
// asked of it — including whether a preflight went out first, and whether the
// real request was sent at all.
class CorsNetwork final : public Hummingbird::INetwork {
public:
    struct Call {
        std::string url;
        std::string method;
        Hummingbird::Core::HttpHeaders headers;
    };

    void set_headers(Hummingbird::Core::HttpHeaders headers) { response_headers_ = std::move(headers); }
    void set_preflight_headers(Hummingbird::Core::HttpHeaders headers) { preflight_headers_ = std::move(headers); }

    void get(const std::string& url, std::function<void(Hummingbird::NetworkResponse)> callback,
             const Hummingbird::NetworkRequestOptions& options = {}) override {
        request(url, "GET", {}, std::move(callback), options);
    }
    void post(const std::string& url, std::string_view body,
              std::function<void(Hummingbird::NetworkResponse)> callback,
              const Hummingbird::NetworkRequestOptions& options = {}) override {
        request(url, "POST", body, std::move(callback), options);
    }
    void request(const std::string& url, std::string_view method, std::string_view,
                 std::function<void(Hummingbird::NetworkResponse)> callback,
                 const Hummingbird::NetworkRequestOptions& options = {}) override {
        calls.push_back(Call{url, std::string(method), options.headers});
        const bool preflight = method == "OPTIONS";
        Hummingbird::NetworkResponse response;
        response.url = url;
        response.effective_url = url;
        response.status = 200;
        response.headers = preflight ? preflight_headers_ : response_headers_;
        response.body = preflight ? "" : "SECRET";
        if (callback) callback(std::move(response));
    }
    void shutdown() override {}

    std::vector<Call> calls;

private:
    Hummingbird::Core::HttpHeaders response_headers_;
    Hummingbird::Core::HttpHeaders preflight_headers_;
};

// Owns the loader for the whole test: the loader owns the network, so a helper
// that let it go out of scope would leave `net` dangling and every assertion on
// `calls` reading freed memory.
struct CorsFixture {
    CorsNetwork* net = nullptr;
    std::unique_ptr<Hummingbird::Engine::ResourceLoader> loader;

    explicit CorsFixture(Hummingbird::Core::HttpHeaders response_headers,
                         Hummingbird::Core::HttpHeaders preflight_headers = {},
                         std::shared_ptr<Hummingbird::Core::CookieJar> jar = nullptr) {
        auto network = std::make_unique<CorsNetwork>();
        net = network.get();
        net->set_headers(std::move(response_headers));
        net->set_preflight_headers(std::move(preflight_headers));
        loader = std::make_unique<Hummingbird::Engine::ResourceLoader>(std::move(network), nullptr, nullptr, nullptr,
                                                                       std::move(jar));
    }

    // The document is always https://page.test/app, so any other host is
    // cross-origin.
    ScriptFetchResponse run(const ScriptFetchRequest& request) {
        ScriptFetchResponse out;
        loader->fetch_for_script(request, "https://page.test/app",
                                 [&](ScriptFetchResponse response) { out = std::move(response); });
        return out;
    }
};

Hummingbird::Core::HttpHeaders allow_any() {
    Hummingbird::Core::HttpHeaders headers;
    headers.set("Access-Control-Allow-Origin", "*");
    return headers;
}
}  // namespace

// The acceptance criterion, stated exactly: a disallowed cross-origin fetch
// rejects WITHOUT exposing the response — not the body, not the headers, not
// even the status. A page that learns "that origin answered 401" has read
// cross-origin state it was refused.
TEST(FetchTest, ABlockedCrossOriginFetchExposesNothing) {
    CorsFixture fx{{}};  // server says nothing about CORS -- the common case
    ScriptFetchRequest request;
    request.url = "https://api.other.test/secret";
    const auto got = fx.run(request);

    EXPECT_EQ(got.failure, ScriptFetchFailure::CorsBlocked);
    EXPECT_EQ(got.status, 0) << "a blocked response must not leak its status";
    EXPECT_TRUE(got.body.empty()) << "a blocked response must not leak its body";
    EXPECT_TRUE(got.headers.fields().empty()) << "a blocked response must not leak its headers";
    // The request still went out -- CORS gates reading the answer, not sending.
    ASSERT_EQ(fx.net->calls.size(), 1u);
    EXPECT_EQ(fx.net->calls[0].headers.get("Origin"), "https://page.test");
}

TEST(FetchTest, AnAllowedCrossOriginFetchResolvesNormally) {
    CorsFixture fx{allow_any()};
    ScriptFetchRequest request;
    request.url = "https://api.other.test/data";
    const auto got = fx.run(request);
    EXPECT_EQ(got.failure, ScriptFetchFailure::None);
    EXPECT_EQ(got.status, 200);
    EXPECT_EQ(got.body, "SECRET");
}

// A same-origin fetch is not subject to CORS at all, and must not sprout an
// Origin header or need permission.
TEST(FetchTest, SameOriginFetchIsNotSubjectToCors) {
    CorsFixture fx{{}};  // no Allow-Origin at all
    ScriptFetchRequest request;
    request.url = "https://page.test/data";  // same origin as the document
    const auto got = fx.run(request);

    EXPECT_EQ(got.failure, ScriptFetchFailure::None);
    EXPECT_EQ(got.body, "SECRET");
    ASSERT_EQ(fx.net->calls.size(), 1u);
    EXPECT_TRUE(fx.net->calls[0].headers.get("Origin").empty());
}
// Credentials mode drives the cookie jar. The default is same-origin, so a
// cross-origin fetch is anonymous — the page must opt in to send cookies, and
// opting in raises the bar the server has to clear.
TEST(FetchTest, CrossOriginFetchIsAnonymousUnlessCredentialsAreRequested) {
    auto jar = std::make_shared<Hummingbird::Core::CookieJar>();
    // SameSite=None is required for the cookie to be eligible AT ALL on a
    // cross-site subresource request. credentials: 'include' is necessary but
    // NOT sufficient — SameSite is a separate gate that runs first, and a
    // default (Lax) cookie stays home no matter what the fetch asks for.
    ASSERT_TRUE(jar->store_from_header("https://api.other.test/", "session=secret; Max-Age=3600; SameSite=None; Secure",
                                       Hummingbird::Core::CookieClock::now()));

    Hummingbird::Core::HttpHeaders allow;
    allow.set("Access-Control-Allow-Origin", "*");

    // Default (same-origin) credentials: no Cookie header, even though the jar
    // holds one for that host.
    CorsFixture anon_fx{allow, {}, jar};
    ScriptFetchRequest anonymous;
    anonymous.url = "https://api.other.test/data";
    const auto got = anon_fx.run(anonymous);
    EXPECT_EQ(got.failure, ScriptFetchFailure::None);
    ASSERT_EQ(anon_fx.net->calls.size(), 1u);
    EXPECT_TRUE(anon_fx.net->calls[0].headers.get("Cookie").empty())
        << "default credentials mode must not send cookies";

    // credentials: 'include' against `*` is refused per spec, and the page
    // learns nothing.
    CorsFixture cred_fx{allow, {}, jar};
    ScriptFetchRequest credentialed = anonymous;
    credentialed.credentials = Hummingbird::Core::Cors::Credentials::Include;
    const auto blocked = cred_fx.run(credentialed);
    EXPECT_EQ(blocked.failure, ScriptFetchFailure::CorsBlocked);
    EXPECT_TRUE(blocked.body.empty());
    // ...and this time the cookie WAS sent, which is exactly why the response
    // must not be readable.
    ASSERT_EQ(cred_fx.net->calls.size(), 1u);
    EXPECT_EQ(cred_fx.net->calls[0].headers.get("Cookie"), "session=secret");

    // Naming the origin and allowing credentials is the combination that works.
    Hummingbird::Core::HttpHeaders named;
    named.set("Access-Control-Allow-Origin", "https://page.test");
    named.set("Access-Control-Allow-Credentials", "true");
    CorsFixture ok_fx{named, {}, jar};
    const auto allowed = ok_fx.run(credentialed);
    EXPECT_EQ(allowed.failure, ScriptFetchFailure::None);
    EXPECT_EQ(allowed.body, "SECRET");
}

// The point of a preflight is that the server never SEES the real request until
// it has agreed to it — which matters when that request would delete something.
TEST(FetchTest, ARefusedPreflightNeverSendsTheRealRequest) {
    Hummingbird::Core::HttpHeaders preflight;
    preflight.set("Access-Control-Allow-Origin", "https://page.test");  // but no Allow-Methods
    CorsFixture fx{{}, preflight};
    ScriptFetchRequest request;
    request.url = "https://api.other.test/thing";
    request.method = "DELETE";
    const auto got = fx.run(request);

    EXPECT_EQ(got.failure, ScriptFetchFailure::CorsBlocked);
    ASSERT_EQ(fx.net->calls.size(), 1u) << "the DELETE must not have been sent";
    EXPECT_EQ(fx.net->calls[0].method, "OPTIONS");
    EXPECT_EQ(fx.net->calls[0].headers.get("Access-Control-Request-Method"), "DELETE");
}

TEST(FetchTest, AnApprovedPreflightIsFollowedByTheRealRequest) {
    Hummingbird::Core::HttpHeaders preflight;
    preflight.set("Access-Control-Allow-Origin", "https://page.test");
    preflight.set("Access-Control-Allow-Methods", "DELETE");
    Hummingbird::Core::HttpHeaders allow;
    allow.set("Access-Control-Allow-Origin", "https://page.test");

    CorsFixture fx{allow, preflight};
    ScriptFetchRequest request;
    request.url = "https://api.other.test/thing";
    request.method = "DELETE";
    const auto got = fx.run(request);

    EXPECT_EQ(got.failure, ScriptFetchFailure::None);
    ASSERT_EQ(fx.net->calls.size(), 2u);
    EXPECT_EQ(fx.net->calls[0].method, "OPTIONS");
    EXPECT_EQ(fx.net->calls[1].method, "DELETE") << "the approved request must retain the page's method";
    // The preflight itself is never credentialed: "may I" must not depend on
    // who is logged in.
    EXPECT_TRUE(fx.net->calls[0].headers.get("Cookie").empty());
}

TEST(FetchTest, AnEmptyBodyPostIsStillSentAsPost) {
    CorsFixture fx{{}};
    ScriptFetchRequest request;
    request.url = "https://page.test/submit";  // same origin: no preflight noise
    request.method = "POST";

    const auto got = fx.run(request);

    EXPECT_EQ(got.failure, ScriptFetchFailure::None);
    ASSERT_EQ(fx.net->calls.size(), 1u);
    EXPECT_EQ(fx.net->calls[0].method, "POST");
}

TEST(FetchTest, AGetWithABodyIsStillSentAsGet) {
    CorsFixture fx{{}};
    ScriptFetchRequest request;
    request.url = "https://page.test/search";  // same origin: no preflight noise
    request.method = "GET";
    request.body = "query=hummingbird";

    const auto got = fx.run(request);

    EXPECT_EQ(got.failure, ScriptFetchFailure::None);
    ASSERT_EQ(fx.net->calls.size(), 1u);
    EXPECT_EQ(fx.net->calls[0].method, "GET");
}

// A simple request goes straight out: preflighting a plain GET would double
// every cross-origin request in the browser for no security gain.
TEST(FetchTest, ASimpleCrossOriginRequestIsNotPreflighted) {
    CorsFixture fx{allow_any()};
    ScriptFetchRequest request;
    request.url = "https://api.other.test/data";
    const auto got = fx.run(request);

    EXPECT_EQ(got.failure, ScriptFetchFailure::None);
    ASSERT_EQ(fx.net->calls.size(), 1u);
    EXPECT_EQ(fx.net->calls[0].method, "GET");
}

// --- CORS across redirect hops (story 9.2.3) ---------------------------------
//
// This is the half of CORS that is invisible in a same-origin test suite, and
// the classic way a strict implementation turns out not to be strict: the check
// is applied to the first request and the chain is then followed blind.

namespace {
// A server that redirects per-URL and answers per-URL, with independently
// settable headers for each hop — so a chain can be allowed at one hop and
// refused at the next.
class RedirectingCorsNetwork final : public Hummingbird::INetwork {
public:
    void redirect(const std::string& from, const std::string& to, Hummingbird::Core::HttpHeaders headers = {}) {
        hops_[from] = Hop{to, std::move(headers)};
    }
    void answer(const std::string& url, Hummingbird::Core::HttpHeaders headers) { answers_[url] = std::move(headers); }

    void get(const std::string& url, std::function<void(Hummingbird::NetworkResponse)> callback,
             const Hummingbird::NetworkRequestOptions& options = {}) override {
        sent_origins.push_back(std::string(options.headers.get("Origin")));
        visited.push_back(url);
        Hummingbird::NetworkResponse response;
        response.url = url;
        response.effective_url = url;
        if (auto hop = hops_.find(url); hop != hops_.end()) {
            response.status = 302;
            response.headers = hop->second.headers;
            response.headers.set("Location", hop->second.to);
        } else {
            response.status = 200;
            response.body = "SECRET";
            if (auto found = answers_.find(url); found != answers_.end()) {
                response.headers = found->second;
            }
        }
        if (callback) callback(std::move(response));
    }
    void post(const std::string& url, std::string_view, std::function<void(Hummingbird::NetworkResponse)> callback,
              const Hummingbird::NetworkRequestOptions& options = {}) override {
        get(url, std::move(callback), options);
    }
    void shutdown() override {}

    std::vector<std::string> visited;
    std::vector<std::string> sent_origins;

private:
    struct Hop {
        std::string to;
        Hummingbird::Core::HttpHeaders headers;
    };
    std::unordered_map<std::string, Hop> hops_;
    std::unordered_map<std::string, Hummingbird::Core::HttpHeaders> answers_;
};

struct RedirectCorsFixture {
    RedirectingCorsNetwork* net = nullptr;
    std::unique_ptr<Hummingbird::Engine::ResourceLoader> loader;

    RedirectCorsFixture() {
        auto network = std::make_unique<RedirectingCorsNetwork>();
        net = network.get();
        loader = std::make_unique<Hummingbird::Engine::ResourceLoader>(std::move(network), nullptr, nullptr, nullptr,
                                                                       nullptr);
    }
    ScriptFetchResponse run(const std::string& url) {
        ScriptFetchRequest request;
        request.url = url;
        ScriptFetchResponse out;
        loader->fetch_for_script(request, "https://page.test/app",
                                 [&](ScriptFetchResponse response) { out = std::move(response); });
        return out;
    }
};

Hummingbird::Core::HttpHeaders allow(const char* origin) {
    Hummingbird::Core::HttpHeaders headers;
    headers.set("Access-Control-Allow-Origin", origin);
    return headers;
}
}  // namespace

// The headline case: a cross-origin fetch that 302s to a THIRD origin must be
// evaluated against that third origin. Checking only the first hop would let
// any permissive server act as an open redirect into one that is not.
TEST(FetchTest, EveryRedirectHopIsCorsChecked) {
    RedirectCorsFixture fx;
    // Hop 1 allows us. Hop 2 says nothing — and is where the data lives.
    fx.net->redirect("https://a.test/start", "https://b.test/data", allow("*"));
    fx.net->answer("https://b.test/data", {});

    const auto got = fx.run("https://a.test/start");

    EXPECT_EQ(got.failure, ScriptFetchFailure::CorsBlocked);
    EXPECT_TRUE(got.body.empty());
    EXPECT_EQ(got.status, 0);
    // Both hops were visited — the block is on reading hop 2, not on making it.
    ASSERT_EQ(fx.net->visited.size(), 2u);
    EXPECT_EQ(fx.net->visited[1], "https://b.test/data");
}

// A chain crossing an origin taints the request: from there on it presents
// `Origin: null`, so the next server must opt in to an opaque origin rather
// than to the page that started it. Otherwise a server could read the
// initiator's origin off a hop that never authorized it.
TEST(FetchTest, ACrossOriginRedirectTaintsTheOriginHeader) {
    RedirectCorsFixture fx;
    fx.net->redirect("https://a.test/start", "https://b.test/data", allow("*"));
    fx.net->answer("https://b.test/data", allow("*"));

    const auto got = fx.run("https://a.test/start");
    EXPECT_EQ(got.failure, ScriptFetchFailure::None);

    ASSERT_EQ(fx.net->sent_origins.size(), 2u);
    EXPECT_EQ(fx.net->sent_origins[0], "https://page.test");
    EXPECT_EQ(fx.net->sent_origins[1], "null") << "a tainted hop must not present the initiator's origin";
}

// The acceptance criterion the story names: a chain that returns to the
// initiator's origin is STILL cross-origin. Otherwise a server could launder
// access by bouncing back through the page's own host.
TEST(FetchTest, AChainReturningToTheInitiatorOriginStaysCrossOrigin) {
    RedirectCorsFixture fx;
    fx.net->redirect("https://other.test/start", "https://page.test/data", allow("*"));
    fx.net->answer("https://page.test/data", {});  // same origin as the document, but no opt-in

    const auto got = fx.run("https://other.test/start");

    EXPECT_EQ(got.failure, ScriptFetchFailure::CorsBlocked) << "coming home must not restore same-origin privileges";
    ASSERT_EQ(fx.net->sent_origins.size(), 2u);
    EXPECT_EQ(fx.net->sent_origins[1], "null");
}

// A request that STARTS same-origin and redirects away becomes a CORS request.
// Enforcement keyed off the initial URL alone would miss this entirely.
TEST(FetchTest, ASameOriginRequestThatRedirectsOffOriginBecomesCorsChecked) {
    RedirectCorsFixture fx;
    fx.net->redirect("https://page.test/go", "https://elsewhere.test/data");
    fx.net->answer("https://elsewhere.test/data", {});

    const auto got = fx.run("https://page.test/go");
    EXPECT_EQ(got.failure, ScriptFetchFailure::CorsBlocked);
    ASSERT_EQ(fx.net->visited.size(), 2u);
    // The first hop was same-origin, so it carried no Origin header at all.
    EXPECT_TRUE(fx.net->sent_origins[0].empty());
    EXPECT_FALSE(fx.net->sent_origins[1].empty());
}

// ...and the same shape succeeds when the destination does opt in, so the rule
// above is enforcement rather than a blanket refusal.
TEST(FetchTest, ASameOriginRequestMayRedirectToAConsentingOrigin) {
    RedirectCorsFixture fx;
    fx.net->redirect("https://page.test/go", "https://elsewhere.test/data");
    fx.net->answer("https://elsewhere.test/data", allow("null"));

    const auto got = fx.run("https://page.test/go");
    EXPECT_EQ(got.failure, ScriptFetchFailure::None);
    EXPECT_EQ(got.body, "SECRET");
}

// Story 9.2.4 through the real loader: a permitted cross-origin response still
// does not hand over every header it carries.
TEST(FetchTest, ACrossOriginResponseExposesOnlyPermittedHeaders) {
    Hummingbird::Core::HttpHeaders server;
    server.set("Access-Control-Allow-Origin", "*");
    server.set("Access-Control-Expose-Headers", "X-Total-Count");
    server.set("Content-Type", "application/json");  // safelisted
    server.set("ETag", "\"v1\"");                    // neither safelisted nor named
    server.set("X-Total-Count", "42");               // named
    server.add("Set-Cookie", "session=secret");      // never readable

    CorsFixture fx{server};
    ScriptFetchRequest request;
    request.url = "https://api.other.test/data";
    const auto got = fx.run(request);

    ASSERT_EQ(got.failure, ScriptFetchFailure::None);
    EXPECT_EQ(got.headers.get("Content-Type"), "application/json");
    EXPECT_EQ(got.headers.get("X-Total-Count"), "42");
    EXPECT_TRUE(got.headers.get("ETag").empty()) << "an unlisted header must stay invisible";
    EXPECT_TRUE(got.headers.get("Set-Cookie").empty()) << "Set-Cookie reached script";
}

// ...while a same-origin response exposes ordinary headers but still withholds
// cookie-setting fields. Fetch forbids scripts from reading Set-Cookie even
// when the response came from their own origin.
TEST(FetchTest, ASameOriginResponseStillHidesForbiddenHeaders) {
    Hummingbird::Core::HttpHeaders server;
    server.set("ETag", "\"v1\"");
    server.set("X-Anything", "yes");
    server.add("Set-Cookie", "session=secret; HttpOnly");

    CorsFixture fx{server};
    ScriptFetchRequest request;
    request.url = "https://page.test/data";  // same origin as the document
    const auto got = fx.run(request);

    ASSERT_EQ(got.failure, ScriptFetchFailure::None);
    EXPECT_EQ(got.headers.get("ETag"), "\"v1\"");
    EXPECT_EQ(got.headers.get("X-Anything"), "yes");
    EXPECT_TRUE(got.headers.get("Set-Cookie").empty()) << "Set-Cookie reached same-origin script";
}

// With no fetch sink wired up (most unit tests, and any document without a
// network) the binding must REJECT. The pre-9.1.1 stub returned
// `new Promise(function(){})`, so such a page froze its own logic forever.
TEST(FetchTest, WithoutANetworkFetchRejectsInsteadOfHanging) {
    Hummingbird::Core::ArenaAllocator arena(4096, 4);
    auto root = Hummingbird::DOM::Element::create(arena, "div");
    auto out = Hummingbird::DOM::Element::create(arena, "div");
    out->set_attribute("id", "out");
    out->set_attribute("data-log", "");
    auto* out_ptr = out.get();
    root->append_child(std::move(out));

    Hummingbird::Engine::DocumentScriptHost host;  // no fetch sink installed
    host.reset(root.get(), &arena);
    auto engine = Hummingbird::create_script_engine();
    ASSERT_NE(engine, nullptr);
    engine->bind_host(&host);

    ASSERT_TRUE(engine
                    ->eval(FetchFixture::logger() + "fetch('/anything').then(function () { log('resolved'); },"
                                                    "                        function () { log('rejected'); });",
                           "inline")
                    .ok);
    // Settled by the checkpoint at the end of eval — no tick, no transport.
    EXPECT_EQ(host.get_attribute(out_ptr, "data-log"), "rejected");
    EXPECT_EQ(engine->pending_fetch_count(), 0u);
}
