// Story 9.3.1: the cache at the seam it actually lives on — inside
// ResourceLoader::send_request, alongside cookies, CORS, identity and the
// redirect loop.
//
// These tests assert on what the NETWORK saw, not on what the cache reports
// about itself. A cache that says "hit" while still issuing the request is the
// exact failure mode worth catching, and only the transport can tell them apart
// (see the M8 lesson about stubbing the neighbouring layer).
#include <gtest/gtest.h>

#include <chrono>
#include <future>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "core/net/HttpCache.h"
#include "core/platform_api/INetwork.h"
#include "engine/resources/ResourceLoader.h"
#include "platform/net/StubNetwork.h"

namespace {
using Hummingbird::INetwork;
using Hummingbird::NetworkRequestOptions;
using Hummingbird::NetworkResponse;
using Hummingbird::ScriptFetchRequest;
using Hummingbird::ScriptFetchResponse;
using Hummingbird::Core::HttpCache;
using Hummingbird::Core::HttpHeaders;
using Hummingbird::Engine::ResourceLoader;

// Answers from a script the test writes, and records every request it was
// actually asked to make. The recording is the point: "did the network get
// touched" is the only honest way to test a cache.
class RecordingNetwork : public INetwork {
public:
    struct Call {
        std::string url;
        HttpHeaders headers;
    };

    struct Reply {
        long status = 200;
        std::string body;
        HttpHeaders headers;
    };

    // The answer for any URL without one of its own.
    void set_reply(Reply reply) { reply_ = std::move(reply); }
    // The answer for one specific URL, which is what a redirect test needs: a
    // single global reply would send the target back to itself.
    void set_reply_for(const std::string& url, Reply reply) { per_url_[url] = std::move(reply); }

    void get(const std::string& url, std::function<void(NetworkResponse)> callback,
             const NetworkRequestOptions& options) override {
        calls.push_back({url, options.headers});
        const auto found = per_url_.find(url);
        const Reply& reply = found == per_url_.end() ? reply_ : found->second;
        NetworkResponse response;
        response.url = url;
        response.effective_url = url;
        response.status = reply.status;
        response.headers = reply.headers;
        // A 304 carries no body. Modelling that faithfully is what makes the
        // revival path testable at all.
        if (reply.status != 304) response.body = reply.body;
        callback(std::move(response));
    }

    void post(const std::string& url, std::string_view body, std::function<void(NetworkResponse)> callback,
              const NetworkRequestOptions& options) override {
        (void)body;
        get(url, std::move(callback), options);
    }

    void shutdown() override {}

    std::vector<Call> calls;

private:
    Reply reply_;
    std::map<std::string, Reply> per_url_;
};

// Owns the loader so the network it borrows outlives every callback.
struct CacheFixture {
    RecordingNetwork* net = nullptr;
    std::shared_ptr<HttpCache> cache = std::make_shared<HttpCache>();
    std::unique_ptr<ResourceLoader> loader;

    CacheFixture() {
        auto network = std::make_unique<RecordingNetwork>();
        net = network.get();
        loader = std::make_unique<ResourceLoader>(std::move(network), nullptr, nullptr, nullptr, nullptr, nullptr,
                                                  cache);
    }

    // Script-initiated, because that is the one path that hands a result
    // straight back to the caller. The cache sits below this, on the shared
    // per-hop seam, so what holds here holds for documents too.
    ScriptFetchResponse fetch(const std::string& url) {
        ScriptFetchRequest request;
        request.url = url;
        ScriptFetchResponse out;
        loader->fetch_for_script(request, "https://api.test/app",
                                 [&](ScriptFetchResponse response) { out = std::move(response); });
        return out;
    }
};

HttpHeaders cacheable(const char* cache_control, const char* etag = nullptr) {
    HttpHeaders headers;
    headers.add("Cache-Control", cache_control);
    headers.add("Content-Type", "application/json");
    if (etag) headers.add("ETag", etag);
    return headers;
}
}  // namespace

// The headline claim: the second request never reaches the network.
TEST(HttpCacheIntegrationTest, ASecondRequestForAFreshResourceNeverLeavesTheEngine) {
    CacheFixture fx;
    fx.net->set_reply({200, "PAYLOAD", cacheable("max-age=3600")});

    const auto first = fx.fetch("https://api.test/news");
    EXPECT_EQ(first.status, 200);
    EXPECT_EQ(first.body, "PAYLOAD");
    ASSERT_EQ(fx.net->calls.size(), 1u);

    const auto second = fx.fetch("https://api.test/news");
    EXPECT_EQ(second.status, 200);
    EXPECT_EQ(second.body, "PAYLOAD") << "the body came from the cache";
    EXPECT_EQ(fx.net->calls.size(), 1u) << "the network must not have been asked a second time";
    // `Age` is how a response says it did not come fresh off the wire, and the
    // one standard way a same-origin page can observe a cache hit.
    EXPECT_FALSE(second.headers.get("Age").empty());
}

// `no-store` means what it says, every single time.
TEST(HttpCacheIntegrationTest, NoStoreAlwaysGoesToTheNetwork) {
    CacheFixture fx;
    fx.net->set_reply({200, "SECRET", cacheable("no-store")});

    fx.fetch("https://api.test/private");
    fx.fetch("https://api.test/private");
    fx.fetch("https://api.test/private");
    EXPECT_EQ(fx.net->calls.size(), 3u);
    EXPECT_EQ(fx.cache->stats().entries, 0u);
}

// The acceptance criterion for revalidation, end to end: a stale entry produces
// a CONDITIONAL request, and the 304 that answers it reuses the stored body.
TEST(HttpCacheIntegrationTest, AStaleEntryRevalidatesAndReusesTheBodyOnA304) {
    CacheFixture fx;
    // Stale on arrival — Wikipedia's shape, via Age — but with a validator.
    HttpHeaders first_reply = cacheable("max-age=300", "W/\"v1\"");
    first_reply.add("Age", "11914");
    fx.net->set_reply({200, "ARTICLE", first_reply});

    const auto first = fx.fetch("https://api.test/page");
    EXPECT_EQ(first.body, "ARTICLE");
    ASSERT_EQ(fx.net->calls.size(), 1u);
    EXPECT_TRUE(fx.net->calls[0].headers.get("If-None-Match").empty()) << "nothing to be conditional about yet";

    // The server now says "unchanged" and sends no body at all.
    fx.net->set_reply({304, "", cacheable("max-age=300", "W/\"v1\"")});
    const auto second = fx.fetch("https://api.test/page");

    ASSERT_EQ(fx.net->calls.size(), 2u) << "a stale entry must still ask";
    EXPECT_EQ(fx.net->calls[1].headers.get("If-None-Match"), "W/\"v1\"")
        << "the weak validator must be sent verbatim";
    EXPECT_EQ(second.status, 200) << "the page sees the stored 200, never the 304";
    EXPECT_EQ(second.body, "ARTICLE") << "the body was revived from cache, not resent";
    EXPECT_EQ(fx.cache->stats().not_modified, 1u);
}

// Falls back to If-Modified-Since when that is all the server gave us.
TEST(HttpCacheIntegrationTest, RevalidatesWithIfModifiedSinceWhenThereIsNoEtag) {
    CacheFixture fx;
    HttpHeaders reply = cacheable("max-age=0");
    reply.add("Last-Modified", "Wed, 29 Jul 2026 12:00:00 GMT");
    fx.net->set_reply({200, "DOC", reply});

    fx.fetch("https://api.test/doc");
    fx.fetch("https://api.test/doc");
    ASSERT_EQ(fx.net->calls.size(), 2u);
    EXPECT_EQ(fx.net->calls[1].headers.get("If-Modified-Since"), "Wed, 29 Jul 2026 12:00:00 GMT");
}

// A response the page could not read must not be stored where a later request
// could pick it up. Storing a CORS-blocked response would let the block be
// laundered away by a second attempt.
TEST(HttpCacheIntegrationTest, ACorsBlockedResponseIsNeverStored) {
    CacheFixture fx;
    // No Access-Control-Allow-Origin, and a cross-origin target.
    fx.net->set_reply({200, "SECRET", cacheable("max-age=3600")});
    const auto blocked = fx.fetch("https://elsewhere.test/data");

    EXPECT_EQ(blocked.failure, Hummingbird::ScriptFetchFailure::CorsBlocked);
    EXPECT_EQ(fx.cache->stats().entries, 0u) << "a refused response must not survive in the cache";
    // And the second attempt is a real request, not a cached bypass of the check.
    fx.fetch("https://elsewhere.test/data");
    EXPECT_EQ(fx.net->calls.size(), 2u);
}

// What lands in the cache is what the SERVER said, not what the page was
// allowed to read. Storing the filtered copy would drop
// Access-Control-Allow-Origin, and the re-check on the next hit would then
// block a response the server had plainly allowed — a cache that breaks CORS in
// the safe direction is still a cache that breaks CORS.
TEST(HttpCacheIntegrationTest, ACachedCrossOriginResponseStillPassesItsOwnCorsCheck) {
    CacheFixture fx;
    HttpHeaders reply = cacheable("max-age=3600");
    reply.add("Access-Control-Allow-Origin", "*");
    fx.net->set_reply({200, "DATA", reply});

    const auto first = fx.fetch("https://elsewhere.test/open");
    ASSERT_EQ(first.failure, Hummingbird::ScriptFetchFailure::None);
    EXPECT_EQ(first.body, "DATA");

    const auto second = fx.fetch("https://elsewhere.test/open");
    EXPECT_EQ(second.failure, Hummingbird::ScriptFetchFailure::None)
        << "the cached response must still satisfy the CORS check on reuse";
    EXPECT_EQ(second.body, "DATA");
    EXPECT_EQ(fx.net->calls.size(), 1u);
    // The exposure filter still applies to the cached copy: the page sees the
    // safelist, not the raw stored headers.
    EXPECT_FALSE(second.headers.get("Content-Type").empty());
    EXPECT_TRUE(second.headers.get("Access-Control-Allow-Origin").empty())
        << "ACAO is not safelisted, so the page never sees it — cached or not";
}

// A reload has to mean something once there is a cache. Without this, F5 on a
// page with `max-age=3600` would show the same bytes for an hour.
TEST(HttpCacheIntegrationTest, AReloadRevalidatesEvenAFreshEntry) {
    CacheFixture fx;
    fx.net->set_reply({200, "V1", cacheable("max-age=3600", "\"v1\"")});
    fx.loader->navigate("https://api.test/page");
    ASSERT_EQ(fx.net->calls.size(), 1u);

    // An ordinary navigation to the same URL is served from cache.
    fx.loader->navigate("https://api.test/page");
    EXPECT_EQ(fx.net->calls.size(), 1u);

    // A reload is not.
    ResourceLoader::DocumentRequest reload;
    reload.cache_policy = ResourceLoader::CachePolicy::Revalidate;
    fx.loader->navigate("https://api.test/page", reload);
    ASSERT_EQ(fx.net->calls.size(), 2u) << "F5 must reach the server";
    EXPECT_EQ(fx.net->calls[1].headers.get("If-None-Match"), "\"v1\"")
        << "and it asks conditionally, so an unchanged page still costs no body";
}

// A hard reload is the stronger promise: not even a conditional request, because
// the point is to distrust what is held rather than to confirm it.
TEST(HttpCacheIntegrationTest, AHardReloadDoesNotEvenAskConditionally) {
    CacheFixture fx;
    fx.net->set_reply({200, "V1", cacheable("max-age=3600", "\"v1\"")});
    fx.loader->navigate("https://api.test/page");
    ASSERT_EQ(fx.net->calls.size(), 1u);

    ResourceLoader::DocumentRequest hard;
    hard.cache_policy = ResourceLoader::CachePolicy::Bypass;
    fx.loader->navigate("https://api.test/page", hard);
    ASSERT_EQ(fx.net->calls.size(), 2u);
    EXPECT_TRUE(fx.net->calls[1].headers.get("If-None-Match").empty())
        << "a hard reload asks for the resource, not for confirmation of the one it holds";

    // It still REFRESHES the entry — bypassing the cache is not disabling it.
    fx.loader->navigate("https://api.test/page");
    EXPECT_EQ(fx.net->calls.size(), 2u) << "the fresh response was stored, so the next visit is a hit";
}

// The subresource rule, which is NOT the same at both levels. Browsers used to
// revalidate every subresource on F5 and moved away from it (Chrome ~2017)
// because fifty assets meant fifty conditional requests. A hard reload is where
// the user is asking for the thorough version.
TEST(HttpCacheIntegrationTest, NormalReloadSparesSubresourcesButHardReloadDoesNot) {
    CacheFixture fx;
    fx.net->set_reply({200, "body { }", cacheable("max-age=3600", "\"css1\"")});

    // Drives one navigation the way a Tab does: `begin_navigation_session` clears
    // the resource store first, because that store is a per-DOCUMENT memo of what
    // has already been requested. Without the reset it short-circuits the second
    // request before the HTTP cache is ever consulted — which is a different
    // mechanism entirely, and skipping the reset here would have this test
    // "passing" on the wrong one.
    const auto navigate_and_load_stylesheet = [&](ResourceLoader::CachePolicy policy) {
        fx.loader->reset();
        ResourceLoader::DocumentRequest request;
        request.cache_policy = policy;
        fx.loader->navigate("https://api.test/page", request);
        const size_t after_document = fx.net->calls.size();
        fx.loader->request_stylesheets({"https://api.test/style.css"}, "https://api.test/page");
        return fx.net->calls.size() - after_document;
    };

    // First visit: the stylesheet has to be fetched.
    EXPECT_EQ(navigate_and_load_stylesheet(ResourceLoader::CachePolicy::Default), 1u);

    // A normal reload leaves a fresh subresource alone. This is the behaviour
    // browsers settled on: re-checking every asset made reload the slow way to
    // reload a page.
    EXPECT_EQ(navigate_and_load_stylesheet(ResourceLoader::CachePolicy::Revalidate), 0u)
        << "a normal reload serves a fresh subresource from cache rather than re-checking it";

    // A hard reload reaches it, which is the whole reason it exists.
    EXPECT_EQ(navigate_and_load_stylesheet(ResourceLoader::CachePolicy::Bypass), 1u)
        << "Ctrl+Shift+R must reach the stylesheet";
    EXPECT_TRUE(fx.net->calls.back().headers.get("If-None-Match").empty())
        << "and unconditionally: a hard reload distrusts what it holds";
}

// A cached permanent redirect saves the whole chain, which is why the cache
// belongs INSIDE the redirect loop rather than wrapped around it.
TEST(HttpCacheIntegrationTest, CachesIndividualHopsSoARedirectIsNotRepeated) {
    CacheFixture fx;
    HttpHeaders redirect = cacheable("max-age=3600");
    redirect.add("Location", "https://api.test/final");
    fx.net->set_reply_for("https://api.test/start", {301, "", redirect});
    // The target is deliberately uncacheable, so the only thing the cache can
    // save on the second attempt is the redirect hop itself. That isolates what
    // this test is about.
    fx.net->set_reply_for("https://api.test/final", {200, "PAYLOAD", cacheable("no-store")});

    const auto first = fx.fetch("https://api.test/start");
    EXPECT_EQ(first.body, "PAYLOAD");
    ASSERT_EQ(fx.net->calls.size(), 2u);
    EXPECT_EQ(fx.net->calls[0].url, "https://api.test/start");
    EXPECT_EQ(fx.net->calls[1].url, "https://api.test/final");

    // The 301 is now cached, so the second attempt goes straight to the target.
    const auto second = fx.fetch("https://api.test/start");
    EXPECT_EQ(second.body, "PAYLOAD");
    ASSERT_EQ(fx.net->calls.size(), 3u) << "one call, not two: the redirect hop was not repeated";
    EXPECT_EQ(fx.net->calls[2].url, "https://api.test/final")
        << "the cached 301 was followed without asking for /start again";
}

// The conditional belongs to the hop that had the cached entry. Carrying it
// across a redirect asks the wrong server about the wrong resource.
TEST(HttpCacheIntegrationTest, ConditionalHeadersDoNotSurviveARedirect) {
    CacheFixture fx;
    // Prime a stale entry for /start.
    fx.net->set_reply({200, "OLD", cacheable("max-age=0", "\"v1\"")});
    fx.fetch("https://api.test/start");
    ASSERT_EQ(fx.net->calls.size(), 1u);

    HttpHeaders redirect;
    redirect.add("Location", "https://api.test/moved");
    fx.net->set_reply({302, "", redirect});
    fx.fetch("https://api.test/start");

    ASSERT_GE(fx.net->calls.size(), 3u);
    EXPECT_EQ(fx.net->calls[1].headers.get("If-None-Match"), "\"v1\"") << "the first hop asks conditionally";
    EXPECT_EQ(fx.net->calls[2].url, "https://api.test/moved");
    EXPECT_TRUE(fx.net->calls[2].headers.get("If-None-Match").empty())
        << "the next hop must not inherit the previous URL's validator";
}

// The demo card, driven through the REAL stub network rather than a fake, on the
// same route the page takes (fetch -> loader -> fallback transport). A demo that
// only works when the neighbouring layer is stubbed is how two user-visible
// fetch bugs shipped green earlier in M9; this is the check that would have
// caught them.
TEST(HttpCacheIntegrationTest, TheDemoEndpointDemonstratesWhatTheCardClaims) {
    auto cache = std::make_shared<HttpCache>();
    // The demo host is served by the FALLBACK transport, which is how the app is
    // wired — a primary-only loader would send it to real DNS and fail.
    auto loader = std::make_unique<ResourceLoader>(nullptr, std::make_unique<Hummingbird::Platform::StubNetwork>(),
                                                  nullptr, nullptr, nullptr, nullptr, cache);

    // StubNetwork answers on a thread pool, so this must WAIT rather than read
    // whatever `out` happens to hold. Reading it eagerly returns a
    // default-constructed response whose failure is `None` — a test that looks
    // like it passed while asserting on nothing at all.
    const auto fetch = [&](const char* path) {
        ScriptFetchRequest request;
        request.url = std::string("https://example.dev") + path;
        std::promise<ScriptFetchResponse> promise;
        auto future = promise.get_future();
        loader->fetch_for_script(request, "https://example.dev/m9",
                                 [&promise](ScriptFetchResponse response) { promise.set_value(std::move(response)); });
        return future.get();
    };

    // First click: a real response, and the server says it was its first.
    const auto first = fetch("/api/cache-demo");
    ASSERT_EQ(first.failure, Hummingbird::ScriptFetchFailure::None) << "the demo endpoint must be reachable";
    EXPECT_NE(first.body.find("\"served\":1"), std::string::npos);

    // Three more clicks inside max-age. The card claims the body keeps saying
    // "served #1" — which is only true if the server was never asked again.
    for (int i = 0; i < 3; ++i) {
        const auto again = fetch("/api/cache-demo");
        EXPECT_NE(again.body.find("\"served\":1"), std::string::npos) << "click " << i + 2 << " should be a cache hit";
        EXPECT_FALSE(again.headers.get("Age").empty()) << "a cache hit carries an Age";
    }
    EXPECT_EQ(cache->stats().hits, 3u);

    // The stats endpoint is no-store, so it reports what the server really saw.
    const auto stats = fetch("/api/cache-demo/stats");
    EXPECT_NE(stats.body.find("\"full\":1"), std::string::npos)
        << "the server sent exactly one full response, however many times the page asked";

    // And the control: no-store climbs every time.
    EXPECT_NE(fetch("/api/cache-demo/nostore").body.find("\"served\":1"), std::string::npos);
    EXPECT_NE(fetch("/api/cache-demo/nostore").body.find("\"served\":2"), std::string::npos);
}

// Nothing that carries a session is cached in 9.3.1 — in either direction.
TEST(HttpCacheIntegrationTest, ResponsesCarryingSetCookieAreNotStored) {
    CacheFixture fx;
    HttpHeaders reply = cacheable("max-age=3600");
    reply.add("Set-Cookie", "sid=abc; Path=/");
    fx.net->set_reply({200, "PAGE", reply});

    fx.fetch("https://api.test/login");
    fx.fetch("https://api.test/login");
    EXPECT_EQ(fx.net->calls.size(), 2u) << "a Set-Cookie response must never be replayed from memory";
    EXPECT_EQ(fx.cache->stats().entries, 0u);
}
