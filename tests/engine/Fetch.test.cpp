// Story 9.1.1: fetch() end to end, through every layer it actually crosses —
// QuickJS binding -> IScriptHost -> DocumentScriptHost -> Tab -> ResourceLoader
// -> INetwork, and back the other way on the main thread.
//
// The shape being proven is that a fetch returns a Promise NOW and settles it
// LATER: nothing in this file settles a promise from the call that started it.
#include <gtest/gtest.h>

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/ArenaAllocator.h"
#include "core/dom/Element.h"
#include "core/platform_api/ScriptEngineFactory.h"
#include "core/platform_api/ScriptFetch.h"
#include "engine/script/DocumentScriptHost.h"

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
                    ->eval(FetchFixture::logger() +
                               "fetch('/slow').then(function () { log('resolved'); },"
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
                    ->eval("fetch('/submit', { method: 'post',"
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
}

TEST(FetchTest, JsonParsesAndHeadersAreCaseInsensitive) {
    FetchFixture fx;
    ASSERT_TRUE(fx.engine
                    ->eval(FetchFixture::logger() +
                               "fetch('/api').then(function (r) {"
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
                    ->eval(FetchFixture::logger() +
                               "fetch('/api').then(function (r) {"
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
                    ->eval(FetchFixture::logger() +
                               "fetch('/slow').then(function () { log('resolved'); },"
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
                    ->eval(FetchFixture::logger() +
                               "fetch('/anything').then(function () { log('resolved'); },"
                               "                        function () { log('rejected'); });",
                           "inline")
                    .ok);
    // Settled by the checkpoint at the end of eval — no tick, no transport.
    EXPECT_EQ(host.get_attribute(out_ptr, "data-log"), "rejected");
    EXPECT_EQ(engine->pending_fetch_count(), 0u);
}
