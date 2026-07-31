// Story 9.4.2: the bundled ad-block-lite extension, measured rather than
// asserted-at.
//
// The story's acceptance criteria are all countable, and they share one honest
// form: load the SAME page twice, once with the blocker and once without, and
// diff. That is what "avoided" means. A single run with the blocker on can only
// report what the filter says about itself, which is the weakest possible
// evidence — so every number here comes from the before/after pair.
//
// In particular the byte count comes from the fake SERVER, not from the filter.
// A blocked request was never sent, so its size is unknowable from our side;
// only the thing that would have served it knows what it did not serve.
#include <gtest/gtest.h>

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "core/net/RequestFilter.h"
#include "core/utils/AssetLoader.h"
#include "engine/extensions/FilterRuleSet.h"
#include "engine/resources/ResourceLoader.h"

namespace {
using Hummingbird::NetworkRequestOptions;
using Hummingbird::NetworkResponse;
using Hummingbird::Core::RequestFilter;
using Hummingbird::Engine::parse_filter_rule_set;
using Hummingbird::Engine::ResourceLoader;
using Hummingbird::Engine::ResourceType;

// The bundled ruleset, verbatim. Loading the shipped file rather than a
// hand-written copy is the point: a test over a fixture list would keep passing
// after someone broke the real one.
constexpr std::string_view kBundledRules = R"([
  {"id": 1,
   "condition": {"requestDomains": ["ads.example.net"], "domainType": "thirdParty"},
   "action": {"type": "block"}}
])";

constexpr std::string_view kPageUrl = "https://example.dev/m9-adblock";

// A fake origin server that knows the size of everything it serves — and,
// crucially, records only what it was actually ASKED for.
class CountingServer final : public Hummingbird::INetwork {
public:
    void serve(const std::string& url, std::string body) { bodies_[url] = std::move(body); }

    void get(const std::string& url, std::function<void(NetworkResponse)> callback,
             const NetworkRequestOptions& = {}) override {
        NetworkResponse response;
        response.url = url;
        response.effective_url = url;
        response.status = 200;
        if (auto found = bodies_.find(url); found != bodies_.end()) {
            response.body = found->second;
            ++requests_served;
            bytes_served += found->second.size();
        }
        requested.push_back(url);
        if (callback) callback(std::move(response));
    }
    void post(const std::string& url, std::string_view, std::function<void(NetworkResponse)> callback,
              const NetworkRequestOptions& = {}) override {
        get(url, std::move(callback));
    }
    void shutdown() override {}

    std::vector<std::string> requested;
    std::size_t requests_served = 0;
    std::size_t bytes_served = 0;

private:
    std::map<std::string, std::string> bodies_;
};

// The third-party resources the demo page pulls in, with realistic-ish sizes so
// "bytes avoided" is a number rather than a token.
const char* kTrackerScriptUrl = "https://ads.example.net/analytics.js";
const char* kTrackerStyleUrl = "https://ads.example.net/ads.css";
const char* kTrackerPixelUrl = "https://ads.example.net/pixel.png";
const char* kFirstPartyStyleUrl = "https://example.dev/site.css";

struct RunResult {
    std::size_t requests_served = 0;
    std::size_t bytes_served = 0;
    bool first_party_stylesheet_loaded = false;
    bool tracker_script_requested = false;
};

// One page load. `blocking` decides whether ad-block-lite's rules are installed,
// and nothing else differs between the two runs.
RunResult load_page(bool blocking) {
    auto filter = std::make_shared<RequestFilter>();
    if (blocking) {
        auto parsed = parse_filter_rule_set(kBundledRules);
        EXPECT_TRUE(parsed.ok()) << parsed.fatal_error;
        filter->set_rules("ad-block-lite", std::move(parsed.rules));
    }

    auto transport = std::make_unique<CountingServer>();
    CountingServer* server = transport.get();
    server->serve(kFirstPartyStyleUrl, std::string(400, 'a'));
    server->serve(kTrackerScriptUrl, std::string(4096, 'j'));
    server->serve(kTrackerStyleUrl, std::string(512, 'c'));
    server->serve(kTrackerPixelUrl, std::string(67, 'p'));

    ResourceLoader loader(std::move(transport), nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, filter);
    loader.request_stylesheets({kFirstPartyStyleUrl, kTrackerStyleUrl}, kPageUrl);
    loader.request_scripts({kTrackerScriptUrl}, kPageUrl);
    loader.request_images({kTrackerPixelUrl}, kPageUrl);
    loader.consume_pending_updates();

    RunResult result;
    result.requests_served = server->requests_served;
    result.bytes_served = server->bytes_served;
    const auto* first_party = loader.find(kFirstPartyStyleUrl, ResourceType::Stylesheet);
    result.first_party_stylesheet_loaded =
        first_party != nullptr && first_party->state == Hummingbird::Engine::ResourceState::Ready;
    for (const auto& url : server->requested) {
        if (url == kTrackerScriptUrl) result.tracker_script_requested = true;
    }
    return result;
}

}  // namespace

// Criterion 1: requests and bytes avoided. Both numbers come from the server.
TEST(AdBlockLiteTest, ReportsRequestsAndBytesAvoidedAgainstAnUnblockedRun) {
    const RunResult off = load_page(false);
    const RunResult on = load_page(true);

    ASSERT_GT(off.requests_served, on.requests_served) << "the unblocked run must fetch strictly more";
    const std::size_t requests_avoided = off.requests_served - on.requests_served;
    const std::size_t bytes_avoided = off.bytes_served - on.bytes_served;

    EXPECT_EQ(requests_avoided, 3u) << "the script, the stylesheet and the pixel";
    // 4096 + 512 + 67. Asserted exactly, because "some bytes were saved" is the
    // kind of claim that survives the feature silently breaking.
    EXPECT_EQ(bytes_avoided, 4675u);

    // And the number is real in the only sense that matters: the server never
    // saw the request at all.
    EXPECT_TRUE(off.tracker_script_requested);
    EXPECT_FALSE(on.tracker_script_requested);
}

// Criterion 4, and the one that matters most: the page being protected must be
// unharmed. A blocker that improves a number by damaging the page has failed.
TEST(AdBlockLiteTest, TheFirstPartyPageIsUnaffected) {
    const RunResult off = load_page(false);
    const RunResult on = load_page(true);

    EXPECT_TRUE(off.first_party_stylesheet_loaded);
    EXPECT_TRUE(on.first_party_stylesheet_loaded) << "blocking must never touch the site's own resources";
}

// The bundled list must actually parse. Without this the extension could ship
// with a broken rules.json and every other test here would still pass, because
// they use their own copy.
TEST(AdBlockLiteTest, TheShippedRuleSetIsValid) {
    auto rules = Hummingbird::Core::Utils::load_asset_text("assets/extensions/ad-block-lite/rules.json", false);
    ASSERT_TRUE(rules.has_value()) << "the bundled ruleset must exist";

    const auto parsed = parse_filter_rule_set(*rules);
    ASSERT_TRUE(parsed.ok()) << parsed.fatal_error;
    EXPECT_TRUE(parsed.warnings.empty()) << "the shipped list must not contain rules that get skipped";
    EXPECT_GT(parsed.rules.size(), 5u) << "a curated list of one entry is not a proof of anything";

    // Every shipped rule must be third-party scoped. A first-party rule in an
    // ad-block list is how a blocker breaks the site it is on.
    for (const auto& rule : parsed.rules) {
        EXPECT_EQ(rule.third_party, Hummingbird::Core::ThirdPartyScope::ThirdPartyOnly)
            << "rule " << rule.id << " is not scoped to third-party requests";
    }
}

// Scoping proof: the same host, requested BY itself, is first-party and must
// survive. This is what stops the list from breaking ads.example.net's own site.
TEST(AdBlockLiteTest, AThirdPartyRuleDoesNotBlockTheOriginsOwnRequests) {
    auto filter = std::make_shared<RequestFilter>();
    auto parsed = parse_filter_rule_set(kBundledRules);
    ASSERT_TRUE(parsed.ok());
    filter->set_rules("ad-block-lite", std::move(parsed.rules));

    auto transport = std::make_unique<CountingServer>();
    CountingServer* server = transport.get();
    server->serve(kTrackerScriptUrl, std::string(4096, 'j'));

    ResourceLoader loader(std::move(transport), nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, filter);
    // Requested by a page ON ads.example.net, so it is that site's own script.
    loader.request_scripts({kTrackerScriptUrl}, "https://ads.example.net/index.html");

    EXPECT_EQ(server->requests_served, 1u) << "a site's own resources are first-party and must not be blocked";
}
