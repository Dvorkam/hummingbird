#include "platform/net/StubNetwork.h"

#include <functional>
#include <optional>
#include <utility>

#include "core/utils/AssetLoader.h"
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

    return "<html><body><p>Failed to load, try to refresh?: " + url + "</p></body></html>";
}

void run_stub_request(const std::string& url, std::function<void(NetworkResponse)> cb,
                      std::string_view post_body = {}) {
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
    (void)options;
    if (Hummingbird::Platform::respond_if_stopping(thread_pool_.stopping(), callback, url)) return;

    auto cb = std::move(callback);

    thread_pool_.submit([url, cb = std::move(cb), this]() mutable {
        if (Hummingbird::Platform::respond_if_stopping(thread_pool_.stopping(), cb, url)) return;
        run_stub_request(url, std::move(cb));
    });
}

void StubNetwork::post(const std::string& url, std::string_view body, std::function<void(NetworkResponse)> callback,
                       const NetworkRequestOptions& options) {
    (void)options;
    if (Hummingbird::Platform::respond_if_stopping(thread_pool_.stopping(), callback, url)) return;

    auto cb = std::move(callback);
    const std::string body_copy(body);

    thread_pool_.submit([url, body_copy, cb = std::move(cb), this]() mutable {
        if (Hummingbird::Platform::respond_if_stopping(thread_pool_.stopping(), cb, url)) return;
        run_stub_request(url, std::move(cb), body_copy);
    });
}

}  // namespace Hummingbird::Platform
