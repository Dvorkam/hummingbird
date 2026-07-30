// Story 9.4.1: declarative rules are matched at the ResourceLoader choke point.
//
// The unit tests in core/RequestFilter.test.cpp prove the MATCHER. These prove
// the thing that actually matters to a user: a blocked request never reaches the
// network. So they assert on what the fake server SAW, not on what the filter
// reports about itself — a filter that counts a block while the transport still
// sends the request would pass the former and fail the latter.
#include <gtest/gtest.h>

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/net/RequestFilter.h"
#include "engine/resources/ResourceLoader.h"

namespace {
using Hummingbird::NetworkError;
using Hummingbird::NetworkRequestOptions;
using Hummingbird::NetworkResponse;
using Hummingbird::Core::FilterRule;
using Hummingbird::Core::RequestFilter;
using Hummingbird::Engine::ResourceLoader;
using Hummingbird::Engine::ResourceState;
using Hummingbird::Engine::ResourceType;

// Records every URL it is asked for, and can be told to redirect.
class RecordingNetwork final : public Hummingbird::INetwork {
public:
    void redirect(const std::string& url, std::string location) { hops_[url] = std::move(location); }
    void set_body(const std::string& url, std::string body) { bodies_[url] = std::move(body); }

    void get(const std::string& url, std::function<void(NetworkResponse)> callback,
             const NetworkRequestOptions& = {}) override {
        requested.push_back(url);
        NetworkResponse response;
        response.url = url;
        response.effective_url = url;
        if (auto hop = hops_.find(url); hop != hops_.end()) {
            response.status = 302;
            response.headers.add("Location", hop->second);
        } else {
            response.status = 200;
            if (auto found = bodies_.find(url); found != bodies_.end()) response.body = found->second;
        }
        if (callback) callback(std::move(response));
    }
    void post(const std::string& url, std::string_view, std::function<void(NetworkResponse)> callback,
              const NetworkRequestOptions& = {}) override {
        get(url, std::move(callback));
    }
    void shutdown() override {}

    bool saw(std::string_view url) const {
        for (const auto& seen : requested) {
            if (seen == url) return true;
        }
        return false;
    }

    std::vector<std::string> requested;

private:
    std::unordered_map<std::string, std::string> hops_;
    std::unordered_map<std::string, std::string> bodies_;
};

FilterRule block_domain(std::string domain) {
    FilterRule rule;
    rule.id = 1;
    rule.request_domain = std::move(domain);
    return rule;
}

struct Harness {
    explicit Harness(std::vector<FilterRule> rules) : filter(std::make_shared<RequestFilter>()) {
        filter->set_rules("test-blocker", std::move(rules));
        auto transport = std::make_unique<RecordingNetwork>();
        network = transport.get();
        loader = std::make_unique<ResourceLoader>(std::move(transport), nullptr, nullptr, nullptr, nullptr, nullptr,
                                                  nullptr, filter);
    }

    std::shared_ptr<RequestFilter> filter;
    RecordingNetwork* network = nullptr;
    std::unique_ptr<ResourceLoader> loader;
};

const char* kPage = "https://example.com/index.html";

}  // namespace

TEST(RequestFilterIntegrationTest, ABlockedSubresourceNeverReachesTheNetwork) {
    Harness harness({block_domain("tracker.net")});
    harness.loader->request_scripts({"https://tracker.net/analytics.js", "https://example.com/app.js"}, kPage);

    EXPECT_FALSE(harness.network->saw("https://tracker.net/analytics.js"))
        << "the whole point of the feature: the request is not sent";
    EXPECT_TRUE(harness.network->saw("https://example.com/app.js")) << "the first-party script still loads";
}

// Blocked must be distinguishable from Failed, or every consumer that reads
// "not Ready" as "something broke" reports a problem that does not exist.
TEST(RequestFilterIntegrationTest, ABlockedSubresourceReadsAsBlockedNotFailed) {
    Harness harness({block_domain("tracker.net")});
    harness.loader->request_scripts({"https://tracker.net/analytics.js"}, kPage);
    harness.loader->consume_pending_updates();

    const auto* entry = harness.loader->find("https://tracker.net/analytics.js", ResourceType::Script);
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->state, ResourceState::Blocked);
    EXPECT_NE(entry->state, ResourceState::Failed);
    EXPECT_TRUE(entry->body.empty());
}

TEST(RequestFilterIntegrationTest, AnUnfilteredLoaderBlocksNothing) {
    RecordingNetwork* network = nullptr;
    auto transport = std::make_unique<RecordingNetwork>();
    network = transport.get();
    // No filter at all — the default every existing test runs under.
    ResourceLoader loader(std::move(transport), nullptr, nullptr, nullptr);
    loader.request_scripts({"https://tracker.net/analytics.js"}, kPage);

    EXPECT_TRUE(network->saw("https://tracker.net/analytics.js"));
}

// A redirect is how a filtered request would be trivially laundered if the gate
// only saw the first URL. The engine follows redirects itself, so the gate runs
// per hop — this proves it does.
TEST(RequestFilterIntegrationTest, ARedirectIntoABlockedDomainIsStoppedMidChain) {
    Harness harness({block_domain("tracker.net")});
    harness.network->redirect("https://example.com/go", "https://tracker.net/analytics.js");
    harness.loader->request_scripts({"https://example.com/go"}, kPage);

    EXPECT_TRUE(harness.network->saw("https://example.com/go")) << "the first hop is not blocked";
    EXPECT_FALSE(harness.network->saw("https://tracker.net/analytics.js"))
        << "a 302 must not be able to launder a request past the filter";

    harness.loader->consume_pending_updates();
    const auto* entry = harness.loader->find("https://example.com/go", ResourceType::Script);
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->state, ResourceState::Blocked);
}

// M9 scope limit, asserted at the layer that would show the damage: a blocked
// navigation has no interstitial to show, so it must not be blocked at all.
TEST(RequestFilterIntegrationTest, ADocumentNavigationIsNeverBlocked) {
    Harness harness({block_domain("tracker.net")});
    harness.network->set_body("https://tracker.net/page.html", "<html><body>hi</body></html>");
    harness.loader->navigate("https://tracker.net/page.html");

    EXPECT_TRUE(harness.network->saw("https://tracker.net/page.html"));
    harness.loader->consume_pending_updates();
    const auto* entry = harness.loader->find("https://tracker.net/page.html", ResourceType::Document);
    ASSERT_NE(entry, nullptr);
    EXPECT_EQ(entry->state, ResourceState::Ready);
}

TEST(RequestFilterIntegrationTest, ResourceTypeScopingAppliesToTheRightRequests) {
    FilterRule images_only = block_domain("cdn.example.net");
    images_only.destinations = {Hummingbird::Core::RequestDestination::Image};
    Harness harness({images_only});

    harness.loader->request_images({"https://cdn.example.net/ad.png"}, kPage);
    harness.loader->request_scripts({"https://cdn.example.net/lib.js"}, kPage);

    EXPECT_FALSE(harness.network->saw("https://cdn.example.net/ad.png"));
    EXPECT_TRUE(harness.network->saw("https://cdn.example.net/lib.js"))
        << "a rule scoped to images must leave scripts alone";
}

// The initiator has to reach the matcher for third-party rules to mean
// anything. If it did not, this rule would match its own page's request too.
TEST(RequestFilterIntegrationTest, ThirdPartyRulesSeeTheInitiatingDocument) {
    FilterRule rule;
    rule.id = 1;
    rule.url_filter = "/beacon";
    rule.third_party = Hummingbird::Core::ThirdPartyScope::ThirdPartyOnly;
    Harness harness({rule});

    harness.loader->request_images({"https://tracker.net/beacon.gif", "https://example.com/beacon.gif"}, kPage);

    EXPECT_FALSE(harness.network->saw("https://tracker.net/beacon.gif")) << "third-party beacon blocked";
    EXPECT_TRUE(harness.network->saw("https://example.com/beacon.gif"))
        << "the page's own beacon is first-party and must survive";
}

TEST(RequestFilterIntegrationTest, RulesTakeEffectAndStopTakingEffectWhileTheLoaderLives) {
    Harness harness({block_domain("tracker.net")});
    harness.loader->request_scripts({"https://tracker.net/a.js"}, kPage);
    ASSERT_FALSE(harness.network->saw("https://tracker.net/a.js"));

    // Disabling the extension must actually stop the blocking, not merely stop
    // new rules from being added.
    harness.filter->remove_source("test-blocker");
    harness.loader->request_scripts({"https://tracker.net/b.js"}, kPage);
    EXPECT_TRUE(harness.network->saw("https://tracker.net/b.js"));
}

TEST(RequestFilterIntegrationTest, BlockedRequestsAreCountedForTheMeasurements) {
    Harness harness({block_domain("tracker.net")});
    harness.loader->request_images({"https://tracker.net/1.gif", "https://tracker.net/2.gif"}, kPage);
    harness.loader->request_scripts({"https://example.com/app.js"}, kPage);

    EXPECT_EQ(harness.filter->blocked_count(), 2u);
}
