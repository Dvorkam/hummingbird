// Story 9.5.1 (T-API-RENDER-E2E-1): the CI-runnable end-to-end proof of M9's
// North Star — the browser renders data it fetched itself.
//
// A mock API server (an in-process fake INetwork, the pattern of the M6-M8
// harnesses) serves two flows against the real Tab pipeline:
//   1. same-origin list render  — HNPWA-shaped JSON -> DOM -> painted rows
//   2. cross-origin summary     — Wikipedia-shaped JSON, allowed and blocked
// plus the cache path underneath both.
//
// The live gate against api.hnpwa.com and en.wikipedia.org stays manual (the
// example.dev/m9 demo card). A manual gate proves it works today; this is what
// stops it breaking silently tomorrow.
//
// Everything is asserted on PAINTED TEXT and on WHAT THE SERVER SAW. Both
// choices are deliberate: a DOM assertion passes while a missed rebuild leaves
// the screen stale, and a cache that reports "hit" while still issuing the
// request can only be caught from the transport side.
#include <gtest/gtest.h>

#include <fstream>
#include <functional>
#include <map>
#include <optional>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/dom/Element.h"
#include "core/net/HttpCache.h"
#include "core/net/HttpHeaders.h"
#include "core/platform_api/INetwork.h"
#include "layout/RenderObject.h"
#include "core/platform_api/ResourceProviderFactory.h"
#include "test_utils/HeadlessTabHarness.h"

namespace {
using Hummingbird::INetwork;
using Hummingbird::NetworkRequestOptions;
using Hummingbird::NetworkResponse;
using Hummingbird::Core::HttpCache;
using Hummingbird::Core::HttpHeaders;
using Hummingbird::Test::HeadlessTabHarness;

const std::string kApp = "https://app.test/";
const std::string kListPage = kApp + "list";
const std::string kSummaryPage = kApp + "summary";
const std::string kNewsApi = kApp + "api/news";
const std::string kWikiApi = "https://wiki.test/api/rest_v1/page/summary/Hummingbird";
// Story 9.6.1: one document, several routes.
const std::string kSpaPage = kApp + "spa";

// The story-list payload, in api.hnpwa.com's shape (id/title/points/user/
// comments_count). Titles are kept short so each rendered row fits one 800px
// line — a wrapped row would split the phrase the assertions look for.
constexpr std::string_view kNewsJson = R"JSON([
  {"id":1,"title":"Hummingbird fetches its own data","points":128,"user":"engine","comments_count":12},
  {"id":2,"title":"A promise that settles","points":95,"user":"quickjs","comments_count":7},
  {"id":3,"title":"One budget for the whole chain","points":74,"user":"loader","comments_count":3}
])JSON";

// The summary payload, in en.wikipedia.org/api/rest_v1/page/summary's shape.
constexpr std::string_view kSummaryJson =
    R"JSON({"title":"Hummingbird","extract":"Small birds native to the Americas."})JSON";

std::string read_fixture(const std::string& name) {
    std::ifstream file(std::string(HB_TEST_FIXTURE_DIR) + "/" + name, std::ios::binary);
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

// Absolute-space centre of the first render box whose element carries `id`.
// Mirrors the TodoMVC harness's helper: a click has to be synthesized at real
// coordinates so it goes through the same hit-test the user's would.
std::optional<Hummingbird::Layout::Point> center_of_element(const Hummingbird::Layout::RenderObject* node,
                                                            std::string_view id, float ox = 0.0f, float oy = 0.0f) {
    if (!node) return std::nullopt;
    Hummingbird::Layout::Rect rect = node->get_rect();
    rect.x += ox;
    rect.y += oy;
    if (const auto* element = dynamic_cast<const Hummingbird::DOM::Element*>(node->get_dom_node())) {
        if (const auto* value = element->find_attribute("id"); value && *value == id) {
            return Hummingbird::Layout::Point{rect.x + rect.width * 0.5f, rect.y + rect.height * 0.5f};
        }
    }
    for (const auto& child : node->get_children()) {
        if (auto found = center_of_element(child.get(), id, rect.x, rect.y)) return found;
    }
    return std::nullopt;
}

// The mock API server. It answers like a real one — status, headers and body —
// and records every request it was asked to make, including the headers, so a
// test can prove what did and did not leave the engine.
class MockApiServer : public INetwork {
public:
    struct Call {
        std::string url;
        std::string method;
        HttpHeaders headers;
    };

    // Flipped by the blocked-CORS case: the same resource, minus the one header
    // that admits a cross-origin reader.
    bool wiki_allows_cross_origin = true;

    std::vector<Call> calls;

    std::size_t calls_for(const std::string& url) const {
        std::size_t count = 0;
        for (const auto& call : calls) {
            if (call.url == url) ++count;
        }
        return count;
    }

    const Call* last_call_for(const std::string& url) const {
        for (auto it = calls.rbegin(); it != calls.rend(); ++it) {
            if (it->url == url) return &*it;
        }
        return nullptr;
    }

    void get(const std::string& url, std::function<void(NetworkResponse)> callback,
             const NetworkRequestOptions& options) override {
        respond(url, "GET", options, std::move(callback));
    }

    void post(const std::string& url, std::string_view body, std::function<void(NetworkResponse)> callback,
              const NetworkRequestOptions& options) override {
        (void)body;
        respond(url, "POST", options, std::move(callback));
    }

    void shutdown() override {}

private:
    void respond(const std::string& url, const char* method, const NetworkRequestOptions& options,
                 std::function<void(NetworkResponse)> callback) {
        calls.push_back({url, method, options.headers});

        NetworkResponse response;
        response.url = url;
        response.effective_url = url;
        response.status = 200;

        if (url == kListPage || url == kSummaryPage || url == kSpaPage) {
            // The pages themselves are never cached, so each navigation really
            // re-parses and re-runs the script. That keeps the cache assertions
            // below about the API endpoint and nothing else.
            response.headers.add("Content-Type", "text/html; charset=utf-8");
            response.headers.add("Cache-Control", "no-store");
            const char* fixture = "api_render/story_list.html";
            if (url == kSummaryPage) fixture = "api_render/summary.html";
            if (url == kSpaPage) fixture = "api_render/spa.html";
            response.body = read_fixture(fixture);
        } else if (url == kNewsApi) {
            // HNPWA's real freshness shape: an hour of max-age plus a strong ETag.
            response.headers.add("Content-Type", "application/json");
            response.headers.add("Cache-Control", "max-age=3600");
            response.headers.add("ETag", "\"news-1\"");
            response.body = std::string(kNewsJson);
        } else if (url == kWikiApi) {
            response.headers.add("Content-Type", "application/json");
            response.headers.add("Cache-Control", "max-age=300");
            response.headers.add("ETag", "\"wiki-1\"");
            // `Age` is deliberately NOT exposed: the page must be able to read
            // the etag Wikipedia names and nothing else beyond the safelist.
            response.headers.add("Age", "42");
            if (wiki_allows_cross_origin) {
                response.headers.add("Access-Control-Allow-Origin", "*");
                response.headers.add("Access-Control-Expose-Headers", "etag");
            }
            response.body = std::string(kSummaryJson);
        } else {
            response.status = 404;
            response.headers.add("Content-Type", "text/plain");
            response.body = "not found";
        }

        if (callback) callback(std::move(response));
    }
};

// Owns the harness plus the server it borrows, and drives them together.
struct ApiRenderFixture {
    MockApiServer* server = nullptr;
    std::shared_ptr<HttpCache> cache = std::make_shared<HttpCache>();
    std::unique_ptr<HeadlessTabHarness> harness;

    ApiRenderFixture() {
        auto network = std::make_unique<MockApiServer>();
        server = network.get();
        harness = std::make_unique<HeadlessTabHarness>(
            std::move(network), /*fallback=*/nullptr, Hummingbird::create_resource_provider(), /*decoder=*/nullptr,
            /*script_engine=*/nullptr, /*cookie_jar=*/nullptr, /*storage_manager=*/nullptr,
            /*identity_store=*/nullptr, cache);
    }

    // A fetch is answered on the transport's thread and settled on the main
    // thread by a later tick, so one tick is never enough. Ticking to quiescence
    // is what the app's frame loop does.
    void load(const std::string& url) {
        harness->navigate(url);
        pump();
    }

    void pump(int ticks = 8) {
        for (int i = 0; i < ticks; ++i) harness->tick();
    }

    // Clicks the centre of the element carrying `id`, through the real hit-test
    // and event pipeline. Layout is refreshed first so the box is current.
    bool click_by_id(std::string_view id) {
        harness->tick();
        const auto point = center_of_element(harness->tab().render_root(), id);
        if (!point) return false;
        return harness->dispatch_click(*point).handled;
    }

    // Repaints and reports whether `needle` reached the screen. Inline text is
    // painted as per-word runs, so join them and substring-search rather than
    // matching a single draw call.
    bool painted(std::string_view needle) {
        harness->context().drawn_texts.clear();
        harness->paint();
        std::string joined;
        for (const auto& text : harness->context().drawn_texts) joined += text;
        return joined.find(needle) != std::string::npos;
    }
};
}  // namespace

// Flow 1: the North Star claim itself — a page fetches a list it was not shipped
// with, and the rows it built from the response are what gets painted.
TEST(ApiRenderHarnessTest, SameOriginListRendersTheFetchedRows) {
    ApiRenderFixture fx;
    fx.load(kListPage);

    ASSERT_EQ(fx.server->calls_for(kNewsApi), 1u) << "the page never asked the API";
    EXPECT_FALSE(fx.painted("Loading stories")) << "the placeholder must be replaced, not appended to";

    // Every field of every row, so a partially-decoded response cannot pass.
    EXPECT_TRUE(fx.painted("1. Hummingbird fetches its own data (128 points, 12 comments)"));
    EXPECT_TRUE(fx.painted("2. A promise that settles (95 points, 7 comments)"));
    EXPECT_TRUE(fx.painted("3. One budget for the whole chain (74 points, 3 comments)"));

    // A same-origin fetch is not a CORS request and must not announce itself as
    // one — sending `Origin` here would leak the page's identity for nothing.
    const auto* api_call = fx.server->last_call_for(kNewsApi);
    ASSERT_NE(api_call, nullptr);
    EXPECT_TRUE(api_call->headers.get("Origin").empty());
}

// Flow 2a: a cross-origin fetch the server allows renders, and the page can read
// exactly the headers CORS permits (story 9.2.4) — no more.
TEST(ApiRenderHarnessTest, CrossOriginSummaryRendersWhenTheServerAllowsIt) {
    ApiRenderFixture fx;
    fx.load(kSummaryPage);

    ASSERT_EQ(fx.server->calls_for(kWikiApi), 1u);
    const auto* wiki_call = fx.server->last_call_for(kWikiApi);
    ASSERT_NE(wiki_call, nullptr);
    EXPECT_EQ(wiki_call->headers.get("Origin"), "https://app.test") << "a cross-origin request must say who is asking";

    EXPECT_TRUE(fx.painted("Hummingbird: Small birds native to the Americas."));
    // The server named `etag` in Access-Control-Expose-Headers, so the page may
    // read it; it did not name `Age`, which is not on the safelist either.
    EXPECT_TRUE(fx.painted("etag=\"wiki-1\""));
    EXPECT_TRUE(fx.painted("age=hidden"));
}

// Flow 2b: the same request against a server that does not admit the origin. The
// page must learn that it failed and nothing else — not the status, not the
// headers, and above all not the body.
TEST(ApiRenderHarnessTest, CrossOriginSummaryIsBlockedWhenTheServerDoesNotAllowTheOrigin) {
    ApiRenderFixture fx;
    fx.server->wiki_allows_cross_origin = false;
    fx.load(kSummaryPage);

    // The request itself is still made — CORS is a read check on the response,
    // not a send check — so the block has to be visible on this side of it.
    ASSERT_EQ(fx.server->calls_for(kWikiApi), 1u);

    EXPECT_FALSE(fx.painted("Americas")) << "a blocked response must never reach the page";
    EXPECT_FALSE(fx.painted("etag=")) << "and neither must its headers";
    // A block is reported as an ordinary network failure: `TypeError`, the same
    // rejection a page gets when the host is unreachable.
    EXPECT_TRUE(fx.painted("Could not load: TypeError"));
}

// The cache path, proven where it matters: a second visit renders the same rows
// while the API is never asked again. Asserting only on the cache's own stats
// would pass for a cache that reports a hit and still issues the request.
TEST(ApiRenderHarnessTest, ASecondVisitRendersFromCacheWithoutAskingTheApiAgain) {
    ApiRenderFixture fx;
    fx.load(kListPage);
    ASSERT_EQ(fx.server->calls_for(kNewsApi), 1u);
    ASSERT_TRUE(fx.painted("1. Hummingbird fetches its own data (128 points, 12 comments)"));

    fx.load(kListPage);
    EXPECT_EQ(fx.server->calls_for(kListPage), 2u) << "the page is no-store, so it really was fetched again";
    EXPECT_EQ(fx.server->calls_for(kNewsApi), 1u) << "the API answer was still fresh; nothing should have left";
    EXPECT_TRUE(fx.painted("1. Hummingbird fetches its own data (128 points, 12 comments)"))
        << "the cached body has to render exactly like the live one";
    EXPECT_TRUE(fx.painted("3. One budget for the whole chain (74 points, 3 comments)"));
}

// A cross-origin response is stored BEFORE the exposure filter runs, so reusing
// it re-runs the CORS check against the headers the server really sent. Storing
// the filtered copy would drop `Access-Control-Allow-Origin` and make the
// resource work once and then break — silently, and only on the second visit.
TEST(ApiRenderHarnessTest, ACachedCrossOriginResponseStillPassesCorsOnReuse) {
    ApiRenderFixture fx;
    fx.load(kSummaryPage);
    ASSERT_EQ(fx.server->calls_for(kWikiApi), 1u);
    ASSERT_TRUE(fx.painted("Hummingbird: Small birds native to the Americas."));

    fx.load(kSummaryPage);
    EXPECT_EQ(fx.server->calls_for(kWikiApi), 1u) << "served from cache";
    EXPECT_TRUE(fx.painted("Hummingbird: Small birds native to the Americas."))
        << "the cached cross-origin response must still be readable";
    EXPECT_TRUE(fx.painted("etag=\"wiki-1\"")) << "and the exposure decision must survive the round trip";
    EXPECT_FALSE(fx.painted("Could not load"));
}

// F5 on a page whose API answer is still fresh: the document is revalidated, the
// data is not. The two levels of reload are the user's only control over the
// cache, so a regression here is a cache that cannot be escaped.
TEST(ApiRenderHarnessTest, ReloadRefetchesTheDocumentAndKeepsTheFreshApiAnswer) {
    ApiRenderFixture fx;
    fx.load(kListPage);
    ASSERT_EQ(fx.server->calls_for(kNewsApi), 1u);

    fx.harness->tab().reload();
    fx.pump();

    EXPECT_EQ(fx.server->calls_for(kListPage), 2u) << "a reload always re-asks for the document";
    EXPECT_EQ(fx.server->calls_for(kNewsApi), 1u) << "a normal reload does not reach past the document";
    EXPECT_TRUE(fx.painted("2. A promise that settles (95 points, 7 comments)"));
}

// Story 9.6.1 acceptance, end to end: a page fetches a list, pushState's a
// detail route, renders the detail from a second fetch, and Back returns to the
// list with popstate fired and NO document reload.
//
// The "no reload" half is the whole point of the API, and it is only checkable
// from outside the page: the mock server counts document requests, so a reload
// cannot hide behind a re-render that happens to look right.
TEST(ApiRenderHarnessTest, HistoryApiRoutesBetweenViewsWithoutReloadingTheDocument) {
    ApiRenderFixture fx;
    fx.load(kSpaPage);

    // The list rendered from the first fetch.
    ASSERT_EQ(fx.server->calls_for(kNewsApi), 1u);
    EXPECT_TRUE(fx.painted("LIST: Hummingbird fetches its own data"));
    EXPECT_FALSE(fx.painted("DETAIL:"));

    const std::size_t document_loads = fx.server->calls_for(kSpaPage);
    ASSERT_EQ(document_loads, 1u);

    // --- route to the detail view (pushState + a second fetch) ---------------
    fx.click_by_id("go-detail");
    fx.pump();

    EXPECT_EQ(std::string(fx.harness->tab().requested_url()), kApp + "spa/detail/2")
        << "pushState must move the tab's URL — and to a PATH, which is the case that "
           "cannot be mistaken for hash routing";
    EXPECT_TRUE(fx.painted("DETAIL: A promise that settles (95 points)"))
        << "the detail view renders from its own fetch";
    EXPECT_FALSE(fx.painted("LIST: Hummingbird fetches its own data")) << "the list view was replaced";
    EXPECT_EQ(fx.server->calls_for(kSpaPage), document_loads)
        << "a route change must not re-request the document";

    // --- Back returns to the list -------------------------------------------
    ASSERT_TRUE(fx.harness->go_back());
    fx.pump();

    EXPECT_TRUE(fx.painted("LIST: Hummingbird fetches its own data")) << "popstate re-rendered the list";
    EXPECT_FALSE(fx.painted("DETAIL:"));
    EXPECT_EQ(fx.server->calls_for(kSpaPage), document_loads)
        << "going back to a pushState entry must not reload either";
    // The API answer was already cached, so the list re-render costs nothing.
    EXPECT_EQ(fx.server->calls_for(kNewsApi), 1u);

    // --- Forward goes to the detail again, with its state intact ------------
    ASSERT_TRUE(fx.harness->go_forward());
    fx.pump();
    EXPECT_TRUE(fx.painted("DETAIL: A promise that settles (95 points)"))
        << "forward restores the state object, so the page knows which detail to draw";
    EXPECT_EQ(fx.server->calls_for(kSpaPage), document_loads);
}

// Stage A of the story: hash routing already worked, and must keep working
// alongside pushState rather than being replaced by it.
TEST(ApiRenderHarnessTest, HashRoutingStillWorksAlongsideThePushStateRoutes) {
    ApiRenderFixture fx;
    fx.load(kSpaPage);
    const std::size_t document_loads = fx.server->calls_for(kSpaPage);

    fx.click_by_id("go-hash");
    fx.pump();
    EXPECT_TRUE(fx.painted("HASH: about")) << "a hashchange listener rendered the route";
    EXPECT_EQ(fx.server->calls_for(kSpaPage), document_loads);

    ASSERT_TRUE(fx.harness->go_back());
    fx.pump();
    EXPECT_TRUE(fx.painted("LIST: Hummingbird fetches its own data"));
    EXPECT_EQ(fx.server->calls_for(kSpaPage), document_loads);
}
