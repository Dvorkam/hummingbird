// Story 9.1.3: a request's time budget belongs to the whole request, not to each
// redirect hop.
//
// Before this, the only deadline lived hardcoded inside CurlNetwork. Because the
// engine drives the redirect loop (story 8.3.1), every hop is a separate call, so
// that limit applied per hop: with RedirectPolicy::kMaxHops == 20 and a 15s cap,
// a chain could run for five minutes before anything gave up. A timeout also
// arrived as NetworkError::CurlError, indistinguishable from a host that does not
// resolve.
//
// The clock is injected rather than slept through, so these run in microseconds.
#include <gtest/gtest.h>

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/platform_api/INetwork.h"
#include "engine/resources/NetworkErrorPage.h"
#include "engine/resources/RedirectPolicy.h"
#include "engine/resources/ResourceLoader.h"

namespace {
using Hummingbird::NetworkError;
using Hummingbird::NetworkRequestOptions;
using Hummingbird::NetworkResponse;
using Hummingbird::ScriptFetchFailure;
using Hummingbird::ScriptFetchRequest;
using Hummingbird::Engine::NetworkErrorPage;
using Hummingbird::Engine::ResourceLoader;
using Hummingbird::Engine::ResourceType;

// Redirects forever to a URL it has never used, so only a limit can stop it —
// and records each call's deadline options so the budget can be asserted hop by
// hop.
class EndlessRedirectNetwork final : public Hummingbird::INetwork {
public:
    void get(const std::string& url, std::function<void(NetworkResponse)> callback,
             const NetworkRequestOptions& options = {}) override {
        requests.push_back(Request{url, options.connect_timeout_ms, options.total_timeout_ms});
        NetworkResponse response;
        response.url = url;
        response.effective_url = url;
        response.status = 302;
        response.headers.add("Location", "/hop" + std::to_string(requests.size()));
        if (callback) callback(std::move(response));
    }
    void post(const std::string& url, std::string_view, std::function<void(NetworkResponse)> callback,
              const NetworkRequestOptions& options = {}) override {
        get(url, std::move(callback), options);
    }
    void shutdown() override {}

    struct Request {
        std::string url;
        long connect_timeout_ms = 0;
        long total_timeout_ms = 0;
    };
    std::vector<Request> requests;
};

// Answers once, with a body, recording the deadline it was given.
class SingleAnswerNetwork final : public Hummingbird::INetwork {
public:
    void get(const std::string& url, std::function<void(NetworkResponse)> callback,
             const NetworkRequestOptions& options = {}) override {
        total_timeout_ms = options.total_timeout_ms;
        connect_timeout_ms = options.connect_timeout_ms;
        ++calls;
        NetworkResponse response;
        response.url = url;
        response.effective_url = url;
        response.status = 200;
        response.body = "<html>ok</html>";
        if (callback) callback(std::move(response));
    }
    void post(const std::string& url, std::string_view, std::function<void(NetworkResponse)> callback,
              const NetworkRequestOptions& options = {}) override {
        get(url, std::move(callback), options);
    }
    void shutdown() override {}

    long total_timeout_ms = 0;
    long connect_timeout_ms = 0;
    int calls = 0;
};

// Answers an OPTIONS preflight and then the approved DELETE, recording the
// shrinking total budget presented to each transport call.
class PreflightDeadlineNetwork final : public Hummingbird::INetwork {
public:
    void get(const std::string& url, std::function<void(NetworkResponse)> callback,
             const NetworkRequestOptions& options = {}) override {
        request(url, "GET", {}, std::move(callback), options);
    }
    void post(const std::string& url, std::string_view body, std::function<void(NetworkResponse)> callback,
              const NetworkRequestOptions& options = {}) override {
        request(url, "POST", body, std::move(callback), options);
    }
    void request(const std::string& url, std::string_view method, std::string_view,
                 std::function<void(NetworkResponse)> callback,
                 const NetworkRequestOptions& options = {}) override {
        methods.emplace_back(method);
        total_timeout_ms.push_back(options.total_timeout_ms);
        NetworkResponse response;
        response.url = url;
        response.effective_url = url;
        response.status = method == "OPTIONS" ? 204 : 200;
        response.headers.set("Access-Control-Allow-Origin", "https://page.test");
        if (method == "OPTIONS") {
            response.headers.set("Access-Control-Allow-Methods", "DELETE");
        }
        if (callback) callback(std::move(response));
    }
    void shutdown() override {}

    std::vector<std::string> methods;
    std::vector<long> total_timeout_ms;
};

// A clock the test drives: every reading advances by `step`, so a chain burns
// its budget deterministically without any real waiting.
class SteppingClock {
public:
    explicit SteppingClock(std::chrono::milliseconds step) : step_(step) {}
    std::chrono::steady_clock::time_point operator()() {
        const auto reading = base_ + elapsed_;
        elapsed_ += step_;
        return reading;
    }

private:
    std::chrono::steady_clock::time_point base_{};
    std::chrono::milliseconds elapsed_{0};
    std::chrono::milliseconds step_;
};
}  // namespace

// The headline case. Each hop "takes" 4s against a 15s budget, so the chain must
// stop after 4 hops — nowhere near kMaxHops. Under the old per-hop limit every
// one of the 20 hops would have been issued, each with its own full 15s.
TEST(RequestDeadlineTest, BudgetSpansTheWholeChainNotEachHop) {
    auto network = std::make_unique<EndlessRedirectNetwork>();
    auto* net = network.get();

    ResourceLoader loader(std::move(network), nullptr, nullptr, nullptr, nullptr);
    loader.set_request_deadlines({/*connect_ms*/ 5000, /*total_ms*/ 15000});
    loader.set_deadline_clock(SteppingClock(std::chrono::milliseconds(4000)));

    loader.navigate("https://slow.test/start");
    auto batch = loader.consume_pending_updates();

    // 0ms, 4000, 8000, 12000 all have budget left; 16000 does not.
    EXPECT_EQ(net->requests.size(), 4u);
    EXPECT_LT(net->requests.size(), static_cast<size_t>(Hummingbird::Engine::RedirectPolicy::kMaxHops));
    EXPECT_EQ(batch.document_error, NetworkError::Timeout);
}

// The budget must visibly shrink: each hop is handed only what is left, so the
// transport cannot spend a fresh full limit on every hop.
TEST(RequestDeadlineTest, EachHopIsGivenOnlyTheRemainingBudget) {
    auto network = std::make_unique<EndlessRedirectNetwork>();
    auto* net = network.get();

    ResourceLoader loader(std::move(network), nullptr, nullptr, nullptr, nullptr);
    loader.set_request_deadlines({/*connect_ms*/ 5000, /*total_ms*/ 15000});
    loader.set_deadline_clock(SteppingClock(std::chrono::milliseconds(4000)));

    loader.navigate("https://slow.test/start");
    (void)loader.consume_pending_updates();

    ASSERT_EQ(net->requests.size(), 4u);
    EXPECT_EQ(net->requests[0].total_timeout_ms, 15000);
    EXPECT_EQ(net->requests[1].total_timeout_ms, 11000);
    EXPECT_EQ(net->requests[2].total_timeout_ms, 7000);
    EXPECT_EQ(net->requests[3].total_timeout_ms, 3000);

    // Connecting may not outlast the request: once only 3s remain, a 5s connect
    // budget is meaningless and is clamped down to it.
    EXPECT_EQ(net->requests[0].connect_timeout_ms, 5000);
    EXPECT_EQ(net->requests[3].connect_timeout_ms, 3000);
}

// A request that does not redirect gets the whole budget and is not penalised by
// the machinery that bounds chains.
TEST(RequestDeadlineTest, ASingleHopGetsTheFullBudgetAndSucceeds) {
    auto network = std::make_unique<SingleAnswerNetwork>();
    auto* net = network.get();

    ResourceLoader loader(std::move(network), nullptr, nullptr, nullptr, nullptr);
    loader.set_request_deadlines({/*connect_ms*/ 2500, /*total_ms*/ 9000});
    loader.set_deadline_clock(SteppingClock(std::chrono::milliseconds(0)));

    loader.navigate("https://fast.test/page");
    auto batch = loader.consume_pending_updates();

    EXPECT_EQ(net->calls, 1);
    EXPECT_EQ(net->total_timeout_ms, 9000);
    EXPECT_EQ(net->connect_timeout_ms, 2500);
    EXPECT_EQ(batch.document_error, NetworkError::None);

    auto view = loader.view("https://fast.test/page", ResourceType::Document);
    ASSERT_TRUE(view.has_value());
    EXPECT_EQ(view->body, "<html>ok</html>");
}

TEST(RequestDeadlineTest, PreflightAndApprovedRequestShareOneDeadline) {
    auto network = std::make_unique<PreflightDeadlineNetwork>();
    auto* net = network.get();
    ResourceLoader loader(std::move(network), nullptr, nullptr, nullptr, nullptr);
    loader.set_request_deadlines({/*connect_ms*/ 5000, /*total_ms*/ 15000});
    loader.set_deadline_clock(SteppingClock(std::chrono::milliseconds(4000)));

    ScriptFetchRequest request;
    request.url = "https://api.other.test/item";
    request.method = "DELETE";
    ScriptFetchFailure failure = ScriptFetchFailure::NetworkError;
    loader.fetch_for_script(request, "https://page.test/index.html",
                            [&](auto response) { failure = response.failure; });

    EXPECT_EQ(failure, ScriptFetchFailure::None);
    ASSERT_EQ(net->methods, (std::vector<std::string>{"OPTIONS", "DELETE"}));
    ASSERT_EQ(net->total_timeout_ms.size(), 2u);
    EXPECT_EQ(net->total_timeout_ms[0], 11000);
    EXPECT_EQ(net->total_timeout_ms[1], 7000)
        << "the real request must inherit time spent obtaining preflight approval";
}

// The deadlines are configuration, not a constant buried in the transport: this
// is what lets the tests above run without sleeping, and what a future
// per-request fetch timeout will set.
TEST(RequestDeadlineTest, DeadlinesAreConfigurable) {
    ResourceLoader loader(nullptr, nullptr, nullptr, nullptr, nullptr);
    EXPECT_EQ(loader.request_deadlines().connect_ms, 5000);
    EXPECT_EQ(loader.request_deadlines().total_ms, 15000);

    loader.set_request_deadlines({/*connect_ms*/ 1, /*total_ms*/ 2});
    EXPECT_EQ(loader.request_deadlines().connect_ms, 1);
    EXPECT_EQ(loader.request_deadlines().total_ms, 2);
}

// A timeout must not read as "wrong address". The generic failure page sends the
// reader off to check the URL; a timeout should tell them to try again.
TEST(RequestDeadlineTest, TheErrorPageNamesATimeoutDistinctly) {
    const std::string timed_out = NetworkErrorPage::build("https://slow.test/page", NetworkError::Timeout);
    const std::string unreachable = NetworkErrorPage::build("https://slow.test/page", NetworkError::CurlError);

    EXPECT_NE(timed_out.find("took too long"), std::string::npos);
    EXPECT_NE(timed_out.find("https://slow.test/page"), std::string::npos);
    // ...and it is genuinely a different page, not the same wording reused.
    EXPECT_NE(timed_out, unreachable);
    EXPECT_EQ(unreachable.find("took too long"), std::string::npos);
}
