#include "platform/StubNetwork.h"

#include <functional>
#include <utility>

#include "platform/NetworkRequestUtils.h"

namespace Hummingbird::Platform {

namespace {
std::string build_stub_body(const std::string& url) {
    if (url.rfind("http://example.dev/search", 0) == 0 || url.rfind("https://example.dev/search", 0) == 0) {
        std::string query;
        auto query_pos = url.find('?');
        if (query_pos != std::string::npos && query_pos + 1 < url.size()) {
            query = url.substr(query_pos + 1);
        }
        return R"HTML(
<!doctype html>
<html>
  <head>
    <meta charset="utf-8">
    <title>Hummingbird Search</title>
  </head>
  <body>
    <h1>Search Results</h1>
    <p>Submitted query: <code>)HTML" +
               query + R"HTML(</code></p>
    <p><a href="https://example.dev">Back to example.dev</a></p>
  </body>
</html>
)HTML";
    }

    if (url == "http://example.dev" || url == "https://example.dev") {
        return R"HTML(
<!doctype html>
<html>
  <head>
    <meta charset="utf-8">
    <title>Hummingbird CSS Demo</title>
    <link rel="stylesheet" href="assets/stub.css">
    <style>
      body { margin: 20px; padding: 10px; color: #444; }
      h1, h2, .title { margin: 10px; }
      #lead { font-size: 18px; }
      p { margin: 6px; }
      .hidden { display: none; }
      .boxed { border-width: 20px; border-style: solid; border-color: #cc0000; padding: 4px; }
      .inline-block { display: inline-block; border-width: 1px; border-style: solid; border-color: #008000; padding: 2px; }
      .external-demo { color: #cc0000; }
    </style>
  </head>
  <body>
    <h1 class="title">Example Domain</h1>
    <p id="lead"><strong>Typed CSS demo.</strong> This page exercises selector lists, ids, and colors.</p>
    <h2>Elements</h2>
    <p>Inline <em>emphasis</em> and <code>code</code> samples.</p>
    <p class="boxed">Bordered example paragraph.</p>
    <p><span class="inline-block">Inline</span> <span class="inline-block">Block</span></p>
    <h2>External Stylesheet</h2>
    <p class="external-demo">Text color overrides inline, border comes from linked CSS.</p>
    <p class="hidden">You should not see this paragraph.</p>
    <ul>
      <li>List item one</li>
      <li>List item two</li>
    </ul>
    <h2>Form Controls</h2>
    <form action="https://example.dev/search" method="get">
      <input name="q">
      <button type="submit">Search</button>
    </form>
    <blockquote>Simple blockquote to show default styling.</blockquote>
    <hr>
    <p>This domain is for use in illustrative examples.</p>
  </body>
</html>
)HTML";
    }

    return "<html><body><p>Failed to load, try to refresh?: " + url + "</p></body></html>";
}

void run_stub_request(const std::string& url, std::function<void(NetworkResponse)> cb) {
    std::string body = build_stub_body(url);
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

}  // namespace Hummingbird::Platform
