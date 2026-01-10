#include "platform/StubNetwork.h"

namespace {
std::string build_stub_body(const std::string& url) {
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
    NetworkResponse response;
    response.url = url;
    response.effective_url = url;
    response.status = 200;
    response.body = std::move(body);
    if (cb) cb(std::move(response));
}
}  // namespace

void StubNetwork::shutdown() {
    thread_pool_.shutdown();
}

void StubNetwork::get(const std::string& url, std::function<void(NetworkResponse)> callback) {
    if (thread_pool_.stopping()) {
        if (callback) callback(NetworkResponse{.url = url});
        return;
    }

    auto cb = std::move(callback);

    thread_pool_.submit([url, cb = std::move(cb), this]() mutable {
        if (thread_pool_.stopping()) {
            if (cb) cb(NetworkResponse{.url = url});
            return;
        }
        run_stub_request(url, std::move(cb));
    });
}
