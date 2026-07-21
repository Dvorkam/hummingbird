#include "platform/net/StubNetwork.h"

#include <functional>
#include <optional>
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

    return "<html><body><p>Failed to load, try to refresh?: " + url + "</p></body></html>";
}

// The cookie demo (story 8.1.1). Unlike the other stub pages this one has to see
// the request to be worth anything: it echoes the Cookie header the engine sent
// and issues Set-Cookie headers back, so a reload visibly proves the round trip
// through the real jar rather than through a test fake.
constexpr std::string_view kCookieDemoPath = "/cookies";
constexpr std::string_view kCookieScopedPath = "/cookies/private";

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
        std::string_view pair =
            semi == std::string_view::npos ? header.substr(pos) : header.substr(pos, semi - pos);
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
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            default: out.push_back(c);
        }
    }
    return out;
}

void build_cookie_demo(std::string_view path, const Hummingbird::Core::HttpHeaders& request_headers,
                       NetworkResponse& response) {
    const std::string_view sent = request_headers.get("Cookie");

    if (path == kCookieScopedPath) {
        response.headers.add("Set-Cookie", "hb_private=members-only; Path=/cookies/private; Max-Age=86400");
        response.body =
            "<!doctype html><html><head><meta charset=\"utf-8\"><title>Path-scoped cookie</title></head><body>"
            "<h1>Path-scoped cookie set</h1>"
            "<p>This page issued <code>hb_private=members-only</code> with "
            "<code>Path=/cookies/private</code>.</p>"
            "<p>Cookie header this page received: <code>" +
            (sent.empty() ? std::string("(none)") : html_escape(sent)) +
            "</code></p>"
            "<p>Go back to <a href=\"https://example.dev/cookies\">/cookies</a>: <code>hb_private</code> will be "
            "absent there, because that path is outside its scope. Reload <em>this</em> page and it returns.</p>"
            "<p><a href=\"https://example.dev/m8\">&laquo; Back to the M8 demo</a></p>"
            "</body></html>";
        return;
    }

    // Visit counter: the jar is the only thing carrying state between requests,
    // so an incrementing number proves the cookie really made the round trip.
    long visits = 0;
    if (const std::string previous = cookie_value(sent, "hb_visits"); !previous.empty()) {
        if (auto parsed = Hummingbird::Core::Utils::parse_long(previous, Hummingbird::Core::Utils::NumberParseMode::Strict)) {
            visits = *parsed;
        }
    }
    ++visits;
    // hb_session is a session cookie, so its ABSENCE is the visible proof that
    // the browser restarted: hb_visits (persistent) survives, this one does not.
    const bool continuing_session = !cookie_value(sent, "hb_session").empty();

    response.headers.add("Set-Cookie", "hb_visits=" + std::to_string(visits) + "; Path=/; Max-Age=86400");
    // No Expires/Max-Age: dies with the process, so it is the one that resets on
    // restart once 8.1.4 persists the other.
    response.headers.add("Set-Cookie", "hb_session=live; Path=/");
    // 8.1.2: still sent on requests (it appears in the echo above), but already
    // withheld from the script view, so document.cookie in 8.1.5 will not see it.
    response.headers.add("Set-Cookie", "hb_secret=server-only; Path=/; HttpOnly");
    // Strict rides same-site navigation, which is every link on this demo site.
    response.headers.add("Set-Cookie", "hb_strict=same-site-only; Path=/; SameSite=Strict");

    response.body =
        "<!doctype html><html><head><meta charset=\"utf-8\"><title>Hummingbird M8 &mdash; Cookies</title></head>"
        "<body>"
        "<h1>Cookie round trip</h1>"
        "<p>Cookie header this page received:</p>"
        "<pre><code>" +
        (sent.empty() ? std::string("(none yet &mdash; reload to see one)") : html_escape(sent)) +
        "</code></pre>"
        "<p>Visit count carried by <code>hb_visits</code>: <strong>" +
        std::to_string(visits) +
        "</strong></p>"
        "<p>Reload this page and the number climbs: nothing on the server side remembers you, so the count can only "
        "come back from the browser's own jar.</p>"
        "<p><strong>Session status:</strong> " +
        std::string(continuing_session
                        ? "continuing &mdash; <code>hb_session</code> came back, so this browser has stayed open."
                        : "fresh &mdash; no <code>hb_session</code> was sent, so this is a first visit or a restart.") +
        "</p>"
        "<p>Close the browser and reopen it: the visit count keeps climbing because <code>hb_visits</code> is "
        "persisted to disk, while the session status resets. That split is the whole point of a session cookie.</p>"
        "<ul>"
        "<li><code>hb_visits</code> has <code>Max-Age=86400</code> &mdash; a persistent cookie.</li>"
        "<li><code>hb_session</code> has no expiry &mdash; a session cookie, gone when the browser closes.</li>"
        "<li><code>hb_secret</code> is <code>HttpOnly</code> &mdash; it rides requests (you can see it echoed "
        "above) but is already hidden from the script view. Once <code>document.cookie</code> lands in 8.1.5 it "
        "will be the one cookie JS cannot read.</li>"
        "<li><code>hb_strict</code> is <code>SameSite=Strict</code> &mdash; carried on same-site navigation like "
        "the links here, and withheld from any cross-site request.</li>"
        "</ul>"
        "<p><em>Not yet visible in this demo:</em> the cross-site half of SameSite needs a second origin, and "
        "HttpOnly needs <code>document.cookie</code> (8.1.5). Both are enforced in the jar today &mdash; see "
        "the cookie tests.</p>"
        "<p><a href=\"https://example.dev/cookies/private\">Set a path-scoped cookie &raquo;</a> then come back here "
        "and note it is not sent to this path.</p>"
        "<p><a href=\"https://example.dev/m8\">&laquo; Back to the M8 demo</a></p>"
        "</body></html>";
}

void run_stub_request(const std::string& url, std::function<void(NetworkResponse)> cb,
                      const Hummingbird::Core::HttpHeaders& request_headers, std::string_view post_body = {}) {
    const std::string_view path = stub_url_path(url);
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
