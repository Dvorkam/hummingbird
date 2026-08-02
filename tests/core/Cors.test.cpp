// Story 9.2.1: the CORS decision matrix, as pure functions over a request and a
// set of response headers.
//
// CORS answers "may the page READ this?" — the request usually reaches the
// server either way. That is why the interesting cases are all about what the
// response said, and why a wrong "allowed" here is a data leak rather than a
// broken page.
#include "core/net/Cors.h"

#include <gtest/gtest.h>

#include <string>

namespace {
using Hummingbird::Core::HttpHeaders;
using Hummingbird::Core::Cors::Credentials;
using Hummingbird::Core::Cors::Decision;

HttpHeaders headers_of(std::initializer_list<std::pair<const char*, const char*>> fields) {
    HttpHeaders headers;
    for (const auto& [name, value] : fields) {
        headers.set(name, value);
    }
    return headers;
}

constexpr const char* kOrigin = "https://page.test";
}  // namespace

// --- classification ----------------------------------------------------------

TEST(CorsTest, SameOriginNeedsSchemeHostAndPortToMatch) {
    using Hummingbird::Core::Cors::is_same_origin;
    EXPECT_TRUE(is_same_origin("https://a.test/data", "https://a.test/page"));
    EXPECT_TRUE(is_same_origin("https://a.test:443/data", "https://a.test/page"));

    EXPECT_FALSE(is_same_origin("https://b.test/data", "https://a.test/page"));
    EXPECT_FALSE(is_same_origin("http://a.test/data", "https://a.test/page"));        // scheme
    EXPECT_FALSE(is_same_origin("https://a.test:8443/data", "https://a.test/page"));  // port
    // A subdomain is a DIFFERENT origin, even though it is the same *site* for
    // cookies. Conflating the two is how an origin check becomes useless.
    EXPECT_FALSE(is_same_origin("https://api.a.test/data", "https://a.test/page"));

    // An unparseable or non-web URL has an opaque origin, which matches nothing
    // — including itself. Anything else would disable CORS for those URLs.
    EXPECT_FALSE(is_same_origin("data:text/plain,hi", "data:text/plain,hi"));
    EXPECT_FALSE(is_same_origin("nonsense", "https://a.test/page"));
}

// --- simple vs preflighted ---------------------------------------------------

TEST(CorsTest, SimpleRequestsAreTheOnesAFormCouldAlreadyMake) {
    using Hummingbird::Core::Cors::is_simple_request;
    EXPECT_TRUE(is_simple_request("GET", {}));
    EXPECT_TRUE(is_simple_request("post", {}));  // method comparison is case-insensitive
    EXPECT_TRUE(is_simple_request("HEAD", {}));
    EXPECT_TRUE(is_simple_request("POST", headers_of({{"Content-Type", "application/x-www-form-urlencoded"}})));
    EXPECT_TRUE(is_simple_request("POST", headers_of({{"Content-Type", "text/plain; charset=utf-8"}})));
    EXPECT_TRUE(is_simple_request("GET", headers_of({{"Accept", "application/json"}})));

    // A method no form can produce.
    EXPECT_FALSE(is_simple_request("DELETE", {}));
    EXPECT_FALSE(is_simple_request("PUT", {}));
    // JSON is the canonical preflight trigger: a form could never send it, so
    // permitting it without asking would be a new capability.
    EXPECT_FALSE(is_simple_request("POST", headers_of({{"Content-Type", "application/json"}})));
    EXPECT_FALSE(is_simple_request("GET", headers_of({{"X-Custom", "1"}})));
    EXPECT_FALSE(is_simple_request("GET", headers_of({{"Authorization", "Bearer x"}})));
}

// Headers the ENGINE attaches are not the page's doing. Preflighting on our own
// User-Agent would preflight every cross-origin request in the browser.
TEST(CorsTest, EngineAddedHeadersDoNotTriggerAPreflight) {
    using Hummingbird::Core::Cors::headers_needing_preflight;
    using Hummingbird::Core::Cors::is_simple_request;
    const auto engine_headers = headers_of({{"User-Agent", "Hummingbird/0.9"},
                                            {"Referer", "https://page.test/"},
                                            {"Origin", kOrigin},
                                            {"Cookie", "session=abc"},
                                            {"Sec-CH-UA", "\"Hummingbird\""}});
    EXPECT_TRUE(is_simple_request("GET", engine_headers));
    EXPECT_TRUE(headers_needing_preflight(engine_headers).empty());
}

TEST(CorsTest, PreflightHeaderListIsLowercasedSortedAndDeduped) {
    using Hummingbird::Core::Cors::headers_needing_preflight;
    // Deterministic output matters: 9.2.2 will key a preflight cache on it.
    const auto names = headers_needing_preflight(
        headers_of({{"X-Zebra", "1"}, {"Accept", "*/*"}, {"X-Alpha", "2"}, {"Content-Type", "application/json"}}));
    ASSERT_EQ(names.size(), 3u);
    EXPECT_EQ(names[0], "content-type");
    EXPECT_EQ(names[1], "x-alpha");
    EXPECT_EQ(names[2], "x-zebra");
}

// --- the response check ------------------------------------------------------

TEST(CorsTest, AResponseMustOptInToBeReadable) {
    using Hummingbird::Core::Cors::check_response;
    // Silence is refusal. Most of the web answers this way, which is the whole
    // reason a page cannot read arbitrary sites.
    EXPECT_EQ(check_response({}, kOrigin, Credentials::SameOrigin), Decision::MissingAllowOrigin);

    EXPECT_EQ(check_response(headers_of({{"Access-Control-Allow-Origin", "*"}}), kOrigin, Credentials::SameOrigin),
              Decision::Allowed);
    EXPECT_EQ(check_response(headers_of({{"Access-Control-Allow-Origin", kOrigin}}), kOrigin, Credentials::SameOrigin),
              Decision::Allowed);

    // Naming SOMEONE ELSE is not naming us.
    EXPECT_EQ(check_response(headers_of({{"Access-Control-Allow-Origin", "https://other.test"}}), kOrigin,
                             Credentials::SameOrigin),
              Decision::OriginMismatch);
    // A prefix match is not a match — https://page.test.evil.com must not pass.
    EXPECT_EQ(check_response(headers_of({{"Access-Control-Allow-Origin", "https://page.test.evil.com"}}), kOrigin,
                             Credentials::SameOrigin),
              Decision::OriginMismatch);
}

// The acceptance criterion the story calls out by name.
TEST(CorsTest, WildcardCannotAuthorizeACredentialedRequest) {
    using Hummingbird::Core::Cors::check_response;
    const auto wildcard = headers_of({{"Access-Control-Allow-Origin", "*"}});

    // Anonymous: fine.
    EXPECT_EQ(check_response(wildcard, kOrigin, Credentials::Omit), Decision::Allowed);
    EXPECT_EQ(check_response(wildcard, kOrigin, Credentials::SameOrigin), Decision::Allowed);

    // With cookies: refused. A server answering `*` never looked at who asked,
    // so it cannot have decided to trust THIS user's session — honouring it
    // would hand the page another site's logged-in data.
    EXPECT_EQ(check_response(wildcard, kOrigin, Credentials::Include), Decision::WildcardWithCredentials);
    // Even `*` plus an explicit Allow-Credentials does not rescue it.
    EXPECT_EQ(
        check_response(headers_of({{"Access-Control-Allow-Origin", "*"}, {"Access-Control-Allow-Credentials", "true"}}),
                       kOrigin, Credentials::Include),
        Decision::WildcardWithCredentials);
}

TEST(CorsTest, CredentialedRequestsNeedAllowCredentials) {
    using Hummingbird::Core::Cors::check_response;
    const auto named = headers_of({{"Access-Control-Allow-Origin", kOrigin}});
    EXPECT_EQ(check_response(named, kOrigin, Credentials::Include), Decision::CredentialsNotAllowed);

    auto with_credentials = named;
    with_credentials.set("Access-Control-Allow-Credentials", "true");
    EXPECT_EQ(check_response(with_credentials, kOrigin, Credentials::Include), Decision::Allowed);

    // ...and the same response is fine for an anonymous request either way.
    EXPECT_EQ(check_response(named, kOrigin, Credentials::SameOrigin), Decision::Allowed);
}

// --- preflight ---------------------------------------------------------------

TEST(CorsTest, PreflightMustAllowTheMethod) {
    using Hummingbird::Core::Cors::check_preflight;
    const auto base = headers_of({{"Access-Control-Allow-Origin", kOrigin}});

    EXPECT_EQ(check_preflight(base, kOrigin, Credentials::SameOrigin, "DELETE", {}), Decision::MethodNotAllowed);

    auto allows_delete = base;
    allows_delete.set("Access-Control-Allow-Methods", "GET, POST, DELETE");
    EXPECT_EQ(check_preflight(allows_delete, kOrigin, Credentials::SameOrigin, "DELETE", {}), Decision::Allowed);
    // Listing a different verb is not permission for ours.
    EXPECT_EQ(check_preflight(allows_delete, kOrigin, Credentials::SameOrigin, "PUT", {}), Decision::MethodNotAllowed);

    // GET/HEAD/POST need no listing — a successful preflight implies them.
    EXPECT_EQ(check_preflight(base, kOrigin, Credentials::SameOrigin, "POST", {}), Decision::Allowed);
}

TEST(CorsTest, PreflightMustAllowEveryNonSafelistedHeader) {
    using Hummingbird::Core::Cors::check_preflight;
    const auto wanted = headers_of({{"X-Custom", "1"}, {"Content-Type", "application/json"}});

    auto response = headers_of({{"Access-Control-Allow-Origin", kOrigin}});
    EXPECT_EQ(check_preflight(response, kOrigin, Credentials::SameOrigin, "POST", wanted), Decision::HeaderNotAllowed);

    // Allowing only one of the two is still a refusal.
    response.set("Access-Control-Allow-Headers", "x-custom");
    EXPECT_EQ(check_preflight(response, kOrigin, Credentials::SameOrigin, "POST", wanted), Decision::HeaderNotAllowed);

    response.set("Access-Control-Allow-Headers", "X-Custom, Content-Type");
    EXPECT_EQ(check_preflight(response, kOrigin, Credentials::SameOrigin, "POST", wanted), Decision::Allowed);

    // A wildcard covers them all (for an anonymous request).
    response.set("Access-Control-Allow-Headers", "*");
    EXPECT_EQ(check_preflight(response, kOrigin, Credentials::SameOrigin, "POST", wanted), Decision::Allowed);
}

// --- response header exposure (story 9.2.4) ----------------------------------
//
// The other direction: check_response asks what the SERVER allows for the
// request; this asks which response headers the PAGE may observe.

TEST(CorsTest, TheSafelistIsExposedWithoutBeingAsked) {
    using Hummingbird::Core::Cors::filter_exposed_headers;
    const auto response = headers_of({{"Content-Type", "application/json"},
                                      {"Cache-Control", "max-age=60"},
                                      {"Content-Length", "12"},
                                      {"Last-Modified", "now"},
                                      {"ETag", "\"abc\""},
                                      {"X-Total-Count", "42"}});

    const auto exposed = filter_exposed_headers(response, Credentials::SameOrigin);
    EXPECT_EQ(exposed.get("Content-Type"), "application/json");
    EXPECT_EQ(exposed.get("Cache-Control"), "max-age=60");
    EXPECT_EQ(exposed.get("Content-Length"), "12");
    EXPECT_EQ(exposed.get("Last-Modified"), "now");
    // Not safelisted and not named: invisible, even though the server sent it.
    EXPECT_TRUE(exposed.get("ETag").empty());
    EXPECT_TRUE(exposed.get("X-Total-Count").empty());
}

TEST(CorsTest, ExposeHeadersMakesExactlyTheNamedHeadersReadable) {
    using Hummingbird::Core::Cors::filter_exposed_headers;
    auto response = headers_of(
        {{"ETag", "\"abc\""}, {"X-Total-Count", "42"}, {"X-Secret", "no"}, {"Content-Type", "application/json"}});
    response.set("Access-Control-Expose-Headers", "X-Total-Count, etag");

    const auto exposed = filter_exposed_headers(response, Credentials::SameOrigin);
    // Named (matching is case-insensitive)...
    EXPECT_EQ(exposed.get("X-Total-Count"), "42");
    EXPECT_EQ(exposed.get("ETag"), "\"abc\"");
    // ...safelisted anyway...
    EXPECT_EQ(exposed.get("Content-Type"), "application/json");
    // ...and everything else still withheld. Naming two headers is not naming
    // three.
    EXPECT_TRUE(exposed.get("X-Secret").empty());
}

// The acceptance criterion, and the reason a "forbidden" category exists at all.
TEST(CorsTest, SetCookieIsNeverReadableHoweverTheServerAsks) {
    using Hummingbird::Core::Cors::filter_exposed_headers;
    auto response = headers_of({{"Content-Type", "text/plain"}});
    response.add("Set-Cookie", "session=secret");
    // A server that names Set-Cookie has misunderstood; honouring it would hand
    // the page another origin's session token.
    response.set("Access-Control-Expose-Headers", "set-cookie, *");

    for (auto credentials : {Credentials::Omit, Credentials::SameOrigin, Credentials::Include}) {
        const auto exposed = filter_exposed_headers(response, credentials);
        EXPECT_TRUE(exposed.get("Set-Cookie").empty()) << "Set-Cookie leaked to script";
    }
}

TEST(CorsTest, ExposeHeadersWildcardWorksOnlyForAnonymousRequests) {
    using Hummingbird::Core::Cors::filter_exposed_headers;
    auto response = headers_of({{"ETag", "\"abc\""}, {"X-Total-Count", "42"}});
    response.set("Access-Control-Expose-Headers", "*");

    // Anonymous: `*` means everything not forbidden.
    const auto anonymous = filter_exposed_headers(response, Credentials::SameOrigin);
    EXPECT_EQ(anonymous.get("ETag"), "\"abc\"");
    EXPECT_EQ(anonymous.get("X-Total-Count"), "42");

    // Credentialed: `*` is read as the literal header name "*", because a
    // server exposing "everything" to a logged-in caller has almost certainly
    // not thought about what everything contains.
    const auto credentialed = filter_exposed_headers(response, Credentials::Include);
    EXPECT_TRUE(credentialed.get("ETag").empty());
    EXPECT_TRUE(credentialed.get("X-Total-Count").empty());
}

// A preflight that fails the ordinary origin check fails before method/header
// questions are even asked.
TEST(CorsTest, PreflightStillRequiresTheOriginCheck) {
    using Hummingbird::Core::Cors::check_preflight;
    const auto permissive =
        headers_of({{"Access-Control-Allow-Methods", "DELETE"}, {"Access-Control-Allow-Headers", "*"}});
    EXPECT_EQ(check_preflight(permissive, kOrigin, Credentials::SameOrigin, "DELETE", {}),
              Decision::MissingAllowOrigin);
}
