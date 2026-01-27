#include "platform/StubNetwork.h"

#include <fstream>
#include <functional>
#include <optional>
#include <utility>

#include "core/utils/AssetPath.h"
#include "platform/NetworkRequestUtils.h"

namespace Hummingbird::Platform {

namespace {
std::optional<std::string> load_asset_body(std::string_view relative_path) {
    auto path = Hummingbird::Core::Utils::resolve_asset_path(relative_path);
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return std::nullopt;
    }
    std::string bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return bytes;
}

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

    return load_asset_body(rest);
}

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
        return R"HTML(
<!doctype html>
<html>
  <head>
    <meta charset="utf-8">
    <title>Hummingbird Feature Tour</title>
    <link rel="stylesheet" href="assets/stub.css">
    <style>
      body { margin: 20px; padding: 10px; --brand: #ffd54d; --panel: #2a3aa0; }
      h1, h2, .title { margin: 10px; }
      #lead { font-size: 18px; }
      p { margin: 6px; }
      .section { margin: 12px 0; padding: 8px; border-width: 1px; border-style: solid; border-color: #ddd; }
      .hidden { display: none; }
      .boxed { border-width: 20px; border-style: solid; border-color: #cc0000; padding: 4px; }
      .inline-block { display: inline-block; border-width: 1px; border-style: solid; border-color: #008000; padding: 2px; }
      .external-demo { color: #cc0000; }
      .selector-demo { background-color: #fafafa; }
      .selector-demo * { border: 1px solid #ddd; padding: 2px; }
      .selector-demo .title { color: #cc0033; }
      .align-demo { text-align: center; border: 1px dashed #bbb; padding: 4px; }
      .nowrap-demo { white-space: nowrap; border: 1px dashed #bbb; padding: 4px; width: 180px; }
      .underline-demo { text-decoration: underline; }
      .no-underline a { text-decoration: none; }
      .em-demo { font-size: 18px; margin-top: 1.5em; padding: 1em; border: 1px solid #888; background-color: #f5f5f5; }
      .font-demo-sans { font-family: roboto, sans-serif; }
      .font-demo-mono { font-family: "roboto mono", monospace; }
      .font-demo-bold { font-weight: bold; }
      .font-demo-italic { font-style: italic; }
      .font-demo-bold-italic { font-weight: bold; font-style: italic; }
      .var-demo { color: var(--brand); background-color: var(--panel, #444); padding: 4px; }
      .border-style-demo { border-width: 3px; border-style: outset; border-color: #5b3a12; padding: 6px; }
      .table-align-demo { border: 1px solid #bbb; }
      .table-align-demo td { border: 1px solid #ddd; padding: 4px; }
      .table-align-demo .cell-block { width: 60px; border: 1px solid #888; padding: 2px; background-color: #f3f3f3; }
    </style>
  </head>
  <body bgcolor="#f0f7ff" text="#222222" link="#0066cc" vlink="#663399"
        onload="const target = document.getElementById('js-demo'); if (target) { target.textContent = 'Load event fired.'; }">
    <h1 class="title">Hummingbird Feature Tour</h1>
    <p id="lead"><strong>Purpose:</strong> This page demonstrates everything the browser currently supports.</p>

    <div class="section">
      <h2>Typography & Inline Elements</h2>
      <p>Inline <em>emphasis</em>, <strong>strong</strong>, <code>code</code>, and <span class="inline-block">inline-block</span>.</p>
      <p class="boxed">Bordered example paragraph.</p>
      <p><span class="inline-block">Inline</span> <span class="inline-block">Block</span></p>
      <pre>Preformatted
text stays
aligned.</pre>
    </div>

    <div class="section">
      <h2>CSS Variables</h2>
      <p class="var-demo">Custom property demo using var().</p>
    </div>

    <div class="section">
      <h2>Border Styles</h2>
      <p class="border-style-demo">Outset border demo.</p>
    </div>

    <div class="section">
      <h2>Text Readability (CSS)</h2>
      <p class="align-demo">Centered via CSS text-align.</p>
      <p class="nowrap-demo">This sentence should stay on one line even in a narrow box.</p>
      <p class="underline-demo">Underlined via CSS text-decoration.</p>
      <p class="no-underline"><a href="https://example.dev">Link with underline removed.</a></p>
      <div class="em-demo">Em-based spacing: padding and margin scale with font-size.</div>
    </div>

    <div class="section">
      <h2>Fonts & Czech Characters</h2>
      <p class="font-demo-sans">Sans sample: Příliš žluťoučký kůň úpěl ďábelské ódy.</p>
      <p class="font-demo-mono">Mono sample: 0123456789 !@#$%^&amp;*() [] {};</p>
      <p class="font-demo-bold">Bold sample: Font weight from CSS.</p>
      <p class="font-demo-italic">Italic sample: Font style from CSS.</p>
      <p class="font-demo-bold-italic">Bold italic sample: combined CSS style.</p>
    </div>

    <div class="section">
      <h2>Links & Navigation</h2>
      <p><a href="https://example.dev/search?q=feature-demo">Link to search results</a></p>
    </div>

    <div class="section">
      <h2>Lists</h2>
      <ul>
        <li>Unordered item one</li>
        <li>Unordered item two</li>
      </ul>
      <ol>
        <li>Ordered item one</li>
        <li>Ordered item two</li>
      </ol>
    </div>

    <div class="section">
      <h2>Tables</h2>
      <table>
        <thead>
          <tr><th>Feature</th><th>Status</th></tr>
        </thead>
        <tbody>
          <tr><td>Layout</td><td>Supported</td></tr>
          <tr><td>Paint</td><td>Supported</td></tr>
        </tbody>
      </table>
      <p>Cell block alignment demo:</p>
      <table class="table-align-demo" width="240">
        <tr>
          <td align="right"><div class="cell-block">Right</div></td>
          <td align="center"><div class="cell-block">Center</div></td>
        </tr>
      </table>
    </div>

    <div class="section">
      <h2>Images</h2>
      <p>Local assets are loaded via stub network:</p>
      <img src="assets/icons/page_security/secure.png" width="32" height="32" alt="secure icon">
      <img src="assets/icons/page_security/insecure.png" width="32" height="32" alt="insecure icon">
      <img src="assets/logos/hummingbird.svg" width="64" height="64" alt="hummingbird svg">
      <svg width="48" height="48" viewBox="0 0 48 48" aria-label="inline svg demo">
        <rect x="4" y="4" width="40" height="40" rx="6" fill="#4a7a2a"/>
        <circle cx="24" cy="24" r="12" fill="#f4d35e"/>
      </svg>
    </div>

    <div class="section">
      <h2>External Stylesheet</h2>
      <p class="external-demo">Text color overrides inline, border comes from linked CSS.</p>
      <p class="hidden">You should not see this paragraph.</p>
    </div>

    <div class="section selector-demo">
      <h2>Selector Coverage</h2>
      <p class="title">Descendant selector styles this line.</p>
      <div class="card">
        <p><span class="title">Nested title styled via descendant selector.</span></p>
      </div>
    </div>

    <div class="section">
      <h2>HTML Entities</h2>
      <p>Entity demo: &mdash; &nbsp; &amp; &lt; &gt; &quot; &apos;</p>
    </div>

    <div class="section">
      <h2>Custom Elements</h2>
      <my-card>Custom element should render like a generic block.</my-card>
      <x-note>Nested custom element with <span>inline content</span>.</x-note>
    </div>

    <div class="section">
      <h2>Form Controls</h2>
      <form id="search-form" action="https://example.dev/search" method="get">
        <input name="q">
        <button type="submit">Search</button>
      </form>
      <p>External submit button:</p>
      <button type="submit" form="search-form">Search (external)</button>
    </div>

    <div class="section">
      <h2>JavaScript Demo</h2>
      <p id="js-demo">Waiting for script...</p>
      <button onclick="const target = document.getElementById('js-demo'); if (target) { target.textContent = 'Clicked!'; }">Click to run JS</button>
      <script>
        const target = document.getElementById("js-demo");
        if (target) {
          target.textContent = "JS updated this text.";
          target.setAttribute("data-js", "ok");
        }
      </script>
    </div>

    <div class="section">
      <h2>Misc Elements</h2>
      <blockquote>Simple blockquote to show default styling.</blockquote>
      <hr>
      <p>This domain is for use in illustrative examples.</p>
    </div>
  </body>
</html>
)HTML";
    }

    return "<html><body><p>Failed to load, try to refresh?: " + url + "</p></body></html>";
}

void run_stub_request(const std::string& url, std::function<void(NetworkResponse)> cb) {
    if (auto asset_body = try_load_example_asset(url)) {
        NetworkResponse response = Hummingbird::Platform::make_response_with_effective_url(url);
        response.status = 200;
        response.body = std::move(*asset_body);
        if (cb) cb(std::move(response));
        return;
    }

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
