#include "platform/net/StubNetwork.h"

#include <atomic>
#include <functional>
#include <optional>
#include <string>
#include <utility>

#include "core/net/HttpHeaders.h"
#include "core/utils/AssetLoader.h"
#include "core/utils/ParseUtils.h"
#include "core/utils/StringUtils.h"
#include "platform/net/NetworkRequestUtils.h"

namespace Hummingbird::Platform {

namespace {
std::optional<std::string> try_load_example_asset(const std::string& url) {
    constexpr std::string_view kHttp = "http://example.dev/";
    constexpr std::string_view kHttps = "https://example.dev/";
    std::string_view rest;
    if (url.rfind(kHttp, 0) == 0) {
        rest = std::string_view(url).substr(kHttp.size());
    } else if (url.rfind(kHttps, 0) == 0) {
        rest = std::string_view(url).substr(kHttps.size());
    } else {
        return std::nullopt;
    }

    auto query_pos = rest.find('?');
    if (query_pos != std::string_view::npos) {
        rest = rest.substr(0, query_pos);
    }

    if (!rest.starts_with("assets/")) {
        return std::nullopt;
    }

    return Hummingbird::Core::Utils::load_asset_bytes(rest, false);
}

// A JSON endpoint for the fetch demo (story 9.1.1). The stub otherwise only
// serves HTML pages, and fetch needs something shaped like a real API to be
// demonstrable offline. Deliberately mirrors api.hnpwa.com's story-list shape,
// which is M9's proof target, so the demo page's rendering code is the same code
// the live gate exercises.
std::optional<std::string> try_stub_api(const std::string& url) {
    if (url != "http://example.dev/api/news" && url != "https://example.dev/api/news") {
        return std::nullopt;
    }
    return std::string(R"JSON([
  {"id":1,"title":"Hummingbird can fetch its own data now","points":128,"user":"engine","comments_count":12},
  {"id":2,"title":"A promise that settles is better than one that does not","points":95,"user":"quickjs","comments_count":7},
  {"id":3,"title":"One time budget for the whole redirect chain","points":74,"user":"loader","comments_count":3},
  {"id":4,"title":"Every document gets a fresh global","points":61,"user":"teardown","comments_count":5}
])JSON");
}

std::string build_stub_body(const std::string& url, std::string_view post_body = {}) {
    if (url.rfind("http://example.dev/search", 0) == 0 || url.rfind("https://example.dev/search", 0) == 0) {
        std::string query;
        auto query_pos = url.find('?');
        if (query_pos != std::string::npos && query_pos + 1 < url.size()) {
            query = url.substr(query_pos + 1);
        } else if (!post_body.empty()) {
            query.assign(post_body.begin(), post_body.end());
        }
        return R"HTML(
<!doctype html>
<html>
  <head>
    <meta charset="utf-8">
    <title>Hummingbird Search</title>
  </head>
  <body onload="const target = document.getElementById('js-demo'); if (target) { target.textContent = 'Load event fired.'; }">
    <h1>Search Results</h1>
    <p>Submitted query: <code>)HTML" +
               query + R"HTML(</code></p>
    <p><a href="https://example.dev">Back to example.dev</a></p>
  </body>
</html>
)HTML";
    }

    if (auto api = try_stub_api(url)) {
        return *api;
    }

    if (url == "http://example.dev" || url == "https://example.dev") {
        if (auto html = Hummingbird::Core::Utils::load_asset_text("assets/stub/example.dev.html", false)) {
            return *html;
        }
        return "<html><body><p>Missing stub asset: assets/stub/example.dev.html</p></body></html>";
    }

    // Serve demo subpages: example.dev/<name> -> assets/stub/pages/<name>.html.
    constexpr std::string_view kHttpPrefix = "http://example.dev/";
    constexpr std::string_view kHttpsPrefix = "https://example.dev/";
    std::string_view page;
    if (url.rfind(kHttpsPrefix, 0) == 0) {
        page = std::string_view(url).substr(kHttpsPrefix.size());
    } else if (url.rfind(kHttpPrefix, 0) == 0) {
        page = std::string_view(url).substr(kHttpPrefix.size());
    }
    if (auto query_pos = page.find('?'); query_pos != std::string_view::npos) {
        page = page.substr(0, query_pos);
    }
    if (!page.empty() && page.find('/') == std::string_view::npos && page.find("..") == std::string_view::npos) {
        std::string page_path = "assets/stub/pages/" + std::string(page) + ".html";
        if (auto html = Hummingbird::Core::Utils::load_asset_text(page_path, false)) {
            return *html;
        }
    }

    // A non-demo host only reaches the stub as a fallback after the real network
    // failed. The stub has nothing real to serve, so return an empty body and let
    // ResourceLoader render the proper network-error page (8.3.2). Unknown
    // example.dev subpages keep the demo "failed to load" note.
    const bool is_example_dev = url.rfind("http://example.dev", 0) == 0 || url.rfind("https://example.dev", 0) == 0;
    if (!is_example_dev) {
        return {};
    }
    return "<html><body><p>Failed to load, try to refresh?: " + url + "</p></body></html>";
}

// The cookie demo (story 8.1.1). Unlike the other stub pages this one has to see
// the request to be worth anything: it echoes the Cookie header the engine sent
// and issues Set-Cookie headers back, so a reload visibly proves the round trip
// through the real jar rather than through a test fake.
constexpr std::string_view kCookieDemoPath = "/cookies";
constexpr std::string_view kCookieScopedPath = "/cookies/private";

// The cache demo (story 9.3.1). A cache is invisible by nature, which is exactly
// what makes it hard to trust: "it was fast" is not evidence. These endpoints
// count how many times the SERVER was actually reached, so the demo page can
// show the one number a cache exists to hold down — and show it climbing when
// `no-store` says it must.
constexpr std::string_view kCacheDemoPrefix = "/api/cache-demo";
constexpr std::string_view kCacheDemoPath = "/api/cache-demo";
constexpr std::string_view kCacheStatsPath = "/api/cache-demo/stats";
constexpr std::string_view kCacheNoStorePath = "/api/cache-demo/nostore";
// A cacheable SUBRESOURCE, which is the only way the reload levels become
// visible: a normal reload leaves a fresh stylesheet alone and a hard reload
// refetches it. Nothing else on the demo site is cacheable, so without this the
// two reloads are indistinguishable and the demo card would be claiming a
// difference it cannot show.
constexpr std::string_view kCacheStylePath = "/api/cache-demo/style.css";
constexpr std::string_view kCacheStyleEtag = "\"cache-demo-css-v1\"";
// Deliberately constant, so a revalidation always earns a 304 — the case worth
// demonstrating, because it is the one where the round trip happens and the
// payload does not.
constexpr std::string_view kCacheDemoEtag = "\"cache-demo-v1\"";

// Atomic because the stub answers on a thread pool. Process-lifetime, like the
// cache they are reporting on.
std::atomic<int> g_cache_full{0};
std::atomic<int> g_cache_revalidated{0};
std::atomic<int> g_cache_nostore{0};
std::atomic<int> g_cache_css_full{0};
std::atomic<int> g_cache_css_revalidated{0};

bool build_cache_demo(std::string_view path, const Hummingbird::Core::HttpHeaders& request_headers,
                      NetworkResponse& response) {
    if (path == kCacheStatsPath) {
        // Never cached, or it could not report on the cache.
        response.status = 200;
        response.headers.add("Cache-Control", "no-store");
        response.headers.add("Content-Type", "application/json");
        response.body = "{\"full\":" + std::to_string(g_cache_full.load()) +
                        ",\"revalidated\":" + std::to_string(g_cache_revalidated.load()) +
                        ",\"nostore\":" + std::to_string(g_cache_nostore.load()) +
                        ",\"cssFull\":" + std::to_string(g_cache_css_full.load()) +
                        ",\"cssRevalidated\":" + std::to_string(g_cache_css_revalidated.load()) + "}";
        return true;
    }
    if (path == kCacheStylePath) {
        // Requested by the demo page's <link>, so the ENGINE fetches it rather
        // than script — which is the point: this is the subresource the two
        // reload levels treat differently.
        response.headers.add("Cache-Control", "max-age=300");
        response.headers.add("ETag", std::string(kCacheStyleEtag));
        response.headers.add("Content-Type", "text/css");
        if (request_headers.get("If-None-Match") == kCacheStyleEtag) {
            ++g_cache_css_revalidated;
            response.status = 304;
            return true;
        }
        ++g_cache_css_full;
        response.status = 200;
        // Visible, so "did the stylesheet arrive at all" is answerable by looking.
        response.body = ".cache-styled { border-left: 6px solid #b25e00; padding-left: 10px; }\n";
        return true;
    }
    if (path == kCacheNoStorePath) {
        response.status = 200;
        response.headers.add("Cache-Control", "no-store");
        response.headers.add("Content-Type", "application/json");
        response.body = "{\"served\":" + std::to_string(++g_cache_nostore) + "}";
        return true;
    }
    if (path != kCacheDemoPath) return false;

    response.headers.add("Cache-Control", "max-age=10");
    response.headers.add("ETag", std::string(kCacheDemoEtag));
    if (request_headers.get("If-None-Match") == kCacheDemoEtag) {
        ++g_cache_revalidated;
        response.status = 304;
        return true;
    }
    response.status = 200;
    response.headers.add("Content-Type", "application/json");
    // The body records WHICH server response this is. A page still showing
    // "served #1" after five clicks is the proof: the server would have said
    // "#5" if it had been asked.
    response.body = "{\"served\":" + std::to_string(++g_cache_full) + "}";
    return true;
}

std::string_view stub_url_path(std::string_view url) {
    for (std::string_view prefix : {"https://example.dev", "http://example.dev"}) {
        if (url.rfind(prefix, 0) != 0) continue;
        std::string_view rest = url.substr(prefix.size());
        if (const size_t cut = rest.find_first_of("?#"); cut != std::string_view::npos) {
            rest = rest.substr(0, cut);
        }
        return rest.empty() ? "/" : rest;
    }
    return {};
}

// Pulls one cookie's value out of a "a=1; b=2" request header.
std::string cookie_value(std::string_view header, std::string_view name) {
    size_t pos = 0;
    while (pos < header.size()) {
        const size_t semi = header.find(';', pos);
        std::string_view pair = semi == std::string_view::npos ? header.substr(pos) : header.substr(pos, semi - pos);
        pos = semi == std::string_view::npos ? header.size() : semi + 1;
        pair = Hummingbird::Core::Utils::trim_ascii_whitespace(pair);
        const size_t eq = pair.find('=');
        if (eq == std::string_view::npos) continue;
        if (Hummingbird::Core::Utils::trim_ascii_whitespace(pair.substr(0, eq)) == name) {
            return std::string(Hummingbird::Core::Utils::trim_ascii_whitespace(pair.substr(eq + 1)));
        }
    }
    return {};
}

std::string html_escape(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (char c : text) {
        switch (c) {
            case '&':
                out += "&amp;";
                break;
            case '<':
                out += "&lt;";
                break;
            case '>':
                out += "&gt;";
                break;
            default:
                out.push_back(c);
        }
    }
    return out;
}

// Shared chrome for the generated cookie pages, matching
// assets/stub/pages/m8.html so they do not look like a different site.
constexpr std::string_view kCookieDemoStyle =
    "<style>"
    "body { background-color: #f4f6fb; color: #1d2433; font-family: roboto, sans-serif; margin: 0; }"
    ".page { max-width: 760px; margin: 0 auto; padding: 28px 24px 48px 24px; }"
    ".eyebrow { color: #b25e00; font-size: 13px; font-weight: bold; letter-spacing: 1px; }"
    "h1 { color: #14213d; margin: 6px 0 10px 0; }"
    "h2 { color: #14213d; font-size: 17px; margin: 0 0 10px 0; }"
    ".intro { color: #3c4658; margin-bottom: 22px; }"
    ".card { background-color: #ffffff; border: 1px solid #cfd8ea; border-radius: 8px; padding: 18px;"
    " margin-bottom: 16px; }"
    ".wire { background-color: #14213d; border-radius: 6px; color: #e8ecf4; padding: 12px 14px; }"
    ".count { color: #b25e00; font-size: 30px; font-weight: bold; margin: 4px 0 8px 0; }"
    ".label { color: #566174; font-size: 13px; letter-spacing: 1px; margin-bottom: 4px; }"
    ".note { color: #566174; font-size: 14px; line-height: 1.5; }"
    ".good { color: #1a7f4b; font-weight: bold; }"
    "code { background-color: #e8ecf4; border-radius: 3px; padding: 1px 4px; }"
    "a { color: #1b5fbf; }"
    "</style>";

void build_cookie_demo(std::string_view path, const Hummingbird::Core::HttpHeaders& request_headers,
                       NetworkResponse& response) {
    const std::string_view sent = request_headers.get("Cookie");

    if (path == kCookieScopedPath) {
        response.headers.add("Set-Cookie", "hb_private=members-only; Path=/cookies/private; Max-Age=86400");
        response.body =
            "<!doctype html><html><head><meta charset=\"utf-8\"><title>Path-scoped cookie</title>" +
            std::string(kCookieDemoStyle) +
            "</head><body><div class=\"page\">"
            "<p class=\"eyebrow\">M8 &mdash; COOKIES</p>"
            "<h1>Path-scoped cookie</h1>"
            "<div class=\"card\">"
            "<h2>This page just set one</h2>"
            "<p><code>hb_private=members-only</code> with <code>Path=/cookies/private</code>.</p>"
            "<p class=\"label\">COOKIE HEADER THIS PAGE RECEIVED</p>"
            "<p class=\"wire\">" +
            (sent.empty() ? std::string("(none)") : html_escape(sent)) +
            "</p></div>"
            "<div class=\"card\"><h2>Now go back</h2>"
            "<p class=\"note\">Return to <a href=\"https://example.dev/cookies\">/cookies</a> and "
            "<code>hb_private</code> is absent, because that path is outside its scope. Reload this page and it "
            "comes back. Path scoping is otherwise invisible.</p></div>"
            "<p><a href=\"https://example.dev/cookies\">&laquo; Back to the cookie demo</a></p>"
            "</div></body></html>";
        return;
    }

    // Visit counter: the jar is the only thing carrying state between requests,
    // so an incrementing number proves the cookie really made the round trip.
    long visits = 0;
    if (const std::string previous = cookie_value(sent, "hb_visits"); !previous.empty()) {
        if (auto parsed =
                Hummingbird::Core::Utils::parse_long(previous, Hummingbird::Core::Utils::NumberParseMode::Strict)) {
            visits = *parsed;
        }
    }
    ++visits;
    // hb_session is a session cookie, so its ABSENCE is the visible proof that
    // the browser restarted: hb_visits (persistent) survives, this one does not.
    const bool continuing_session = !cookie_value(sent, "hb_session").empty();

    response.headers.add("Set-Cookie", "hb_visits=" + std::to_string(visits) + "; Path=/; Max-Age=86400");
    // No Expires/Max-Age: dies with the process, which is what makes the restart
    // demonstration work now that 8.1.4 persists the other one.
    response.headers.add("Set-Cookie", "hb_session=live; Path=/");
    // Sent on requests (it shows in the wire echo) but withheld from
    // document.cookie -- the side-by-side below is the whole point.
    response.headers.add("Set-Cookie", "hb_secret=server-only; Path=/; HttpOnly");
    response.headers.add("Set-Cookie", "hb_strict=same-site-only; Path=/; SameSite=Strict");

    response.body =
        "<!doctype html><html><head><meta charset=\"utf-8\"><title>Hummingbird M8 &mdash; Cookies</title>" +
        std::string(kCookieDemoStyle) +
        "</head><body><div class=\"page\">"
        "<p class=\"eyebrow\">M8 &mdash; THE SESSION KEEPER</p>"
        "<h1>Cookies, end to end</h1>"
        "<p class=\"intro\">Nothing on the server side remembers you. Everything below came back out of the "
        "cookie jar in the browser.</p>"

        "<div class=\"card\">"
        "<p class=\"label\">VISITS</p>"
        "<p class=\"count\">" +
        std::to_string(visits) +
        "</p>"
        "<p class=\"note\">Reload and it climbs. The count lives in <code>hb_visits</code>, which the browser sends "
        "back on every request.</p></div>"

        "<div class=\"card\">"
        "<h2>What the browser sent</h2>"
        "<p class=\"wire\">Cookie: " +
        (sent.empty() ? std::string("(nothing yet &mdash; reload)") : html_escape(sent)) +
        "</p>"
        "<p class=\"note\">This is the jar as it stood <em>before</em> this response. The cookies below are set by "
        "this response, so they land after it &mdash; which is why the visit count here is one behind.</p></div>"

        // The HttpOnly comparison is deliberately between two things measured at
        // the SAME moment: what this response set, and what script can see once
        // it has been stored. Comparing against the request instead would show
        // three differences (the counter advanced, the session cookie arrived,
        // and hb_secret is hidden) with no way to tell which one is the point.
        "<div class=\"card\">"
        "<h2>What this response set</h2>"
        "<p class=\"wire\">hb_visits, hb_session, hb_secret, hb_strict</p>"
        "<h2>What document.cookie can see</h2>"
        "<p class=\"wire\" id=\"js-cookies\">(script did not run)</p>"
        "<p class=\"note\" id=\"httponly-note\">Both lines describe the jar at the same instant, so there is "
        "exactly one difference to find.</p>"
        "</div>"

        "<div class=\"card\"><h2>Session status</h2><p>" +
        std::string(continuing_session
                        ? "<span class=\"good\">Continuing</span> &mdash; <code>hb_session</code> arrived with this "
                          "request, so the browser has stayed open since it was first set."
                        : "<span class=\"good\">Fresh</span> &mdash; no <code>hb_session</code> arrived with this "
                          "request, so this is a first visit or a restart. This response then sets one, which is why "
                          "<code>document.cookie</code> above already shows it.") +
        "</p>"
        "<p class=\"note\">Close the browser and reopen it: the visit count keeps climbing because "
        "<code>hb_visits</code> is written to disk, while the session status resets. <code>hb_session</code> has no "
        "expiry, so it is never persisted. That split is what a session cookie is for.</p></div>"

        "<div class=\"card\"><h2>Scoping</h2>"
        "<p class=\"note\"><a href=\"https://example.dev/cookies/private\">Set a path-scoped cookie &raquo;</a> then "
        "come back here and note it is not sent to this path. <code>hb_strict</code> is "
        "<code>SameSite=Strict</code>: carried on same-site navigation like these links, withheld from any "
        "cross-site request.</p></div>"

        "<p><a href=\"https://example.dev/m8\">&laquo; Back to the M8 demo</a></p>"

        "<script>"
        "var seen = document.cookie;"
        "document.getElementById('js-cookies').textContent = seen === '' ? '(empty)' : seen;"
        "var hidden = seen.indexOf('hb_secret') < 0;"
        "document.getElementById('httponly-note').textContent = hidden"
        " ? 'The response set four cookies; document.cookie sees three. hb_secret is HttpOnly, so script cannot read"
        " it \u2014 that is what stops an XSS payload stealing a session token. It still rides every request, which"
        " is how the server keeps using it.'"
        " : 'hb_secret leaked into document.cookie: HttpOnly is not being enforced.';"
        "</script>"
        "</div></body></html>";
}

void run_stub_request(const std::string& url, std::function<void(NetworkResponse)> cb,
                      const Hummingbird::Core::HttpHeaders& request_headers, std::string_view post_body = {}) {
    const std::string_view path = stub_url_path(url);
    if (path.rfind(kCacheDemoPrefix, 0) == 0) {
        NetworkResponse response = Hummingbird::Platform::make_response_with_effective_url(url);
        if (build_cache_demo(path, request_headers, response)) {
            if (cb) cb(std::move(response));
            return;
        }
    }
    if (path == kCookieDemoPath || path == kCookieScopedPath) {
        NetworkResponse response = Hummingbird::Platform::make_response_with_effective_url(url);
        response.status = 200;
        build_cookie_demo(path, request_headers, response);
        if (cb) cb(std::move(response));
        return;
    }

    if (auto asset_body = try_load_example_asset(url)) {
        NetworkResponse response = Hummingbird::Platform::make_response_with_effective_url(url);
        response.status = 200;
        response.body = std::move(*asset_body);
        if (cb) cb(std::move(response));
        return;
    }

    std::string body = build_stub_body(url, post_body);
    NetworkResponse response = Hummingbird::Platform::make_response_with_effective_url(url);
    response.status = 200;
    response.body = std::move(body);
    if (cb) cb(std::move(response));
}
}  // namespace

void StubNetwork::shutdown() {
    thread_pool_.shutdown();
}

void StubNetwork::get(const std::string& url, std::function<void(NetworkResponse)> callback,
                      const NetworkRequestOptions& options) {
    if (Hummingbird::Platform::respond_if_stopping(thread_pool_.stopping(), callback, url)) return;

    auto cb = std::move(callback);
    Core::HttpHeaders request_headers = options.headers;

    thread_pool_.submit([url, cb = std::move(cb), this, request_headers = std::move(request_headers)]() mutable {
        if (Hummingbird::Platform::respond_if_stopping(thread_pool_.stopping(), cb, url)) return;
        run_stub_request(url, std::move(cb), request_headers);
    });
}

void StubNetwork::post(const std::string& url, std::string_view body, std::function<void(NetworkResponse)> callback,
                       const NetworkRequestOptions& options) {
    if (Hummingbird::Platform::respond_if_stopping(thread_pool_.stopping(), callback, url)) return;

    auto cb = std::move(callback);
    const std::string body_copy(body);
    Core::HttpHeaders request_headers = options.headers;

    thread_pool_.submit(
        [url, body_copy, cb = std::move(cb), this, request_headers = std::move(request_headers)]() mutable {
            if (Hummingbird::Platform::respond_if_stopping(thread_pool_.stopping(), cb, url)) return;
            run_stub_request(url, std::move(cb), request_headers, body_copy);
        });
}

}  // namespace Hummingbird::Platform
