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
      .underline-offset-demo { text-decoration: underline; text-decoration-thickness: 3px; text-underline-offset: 4px; }
      .text-transform-demo { text-transform: uppercase; }
      .letter-spacing-demo { letter-spacing: 2px; }
      .text-indent-demo { text-indent: 18px; border: 1px dashed #bbb; padding: 4px; width: 260px; }
      .text-overflow-demo { white-space: nowrap; text-overflow: ellipsis; width: 190px; border: 1px dashed #bbb; padding: 4px; }
      .word-wrap-demo { word-wrap: break-word; width: 180px; border: 1px dashed #bbb; padding: 4px; }
      .no-underline a { text-decoration: none; }
      .em-demo { font-size: 18px; margin-top: 1.5em; padding: 1em; border: 1px solid #888; background-color: #f5f5f5; }
      .font-demo-sans { font-family: roboto, sans-serif; }
      .font-demo-mono { font-family: "roboto mono", monospace; }
      .font-demo-bold { font-weight: bold; }
      .font-demo-italic { font-style: italic; }
      .font-demo-bold-italic { font-weight: bold; font-style: italic; }
      .var-demo { color: var(--brand); background-color: var(--panel, #444); padding: 4px; }
      .border-style-demo { border-width: 3px; border-style: outset; border-color: #5b3a12; padding: 6px; }
      .border-radius-demo { border-width: 3px; border-style: solid; border-color: #2d4a9b; background-color: #dfe9ff; padding: 10px; width: 220px; border-radius: 14px; }
      .border-sides-demo {
        width: 240px;
        padding: 8px;
        background-color: #f8fbff;
        border-top: 5px solid #2d4a9b;
        border-right: 3px ridge #5b7bb8;
        border-bottom: 2px solid #2d4a9b;
        border-left: 7px inset #8aa2d6;
      }
      .outline-demo { width: 220px; padding: 8px; border: 1px solid #555; outline: 3px solid #2c7a4b; outline-offset: 4px; margin: 12px; }
      .table-align-demo { border: 1px solid #bbb; }
      .table-align-demo td { border: 1px solid #ddd; padding: 4px; }
      .table-align-demo .cell-block { width: 60px; border: 1px solid #888; padding: 2px; background-color: #f3f3f3; }
      .table-balance-demo { width: 100%; border: 1px solid #bbb; }
      .table-balance-demo td { border: 1px solid #ddd; }
      .list-reset { list-style: none; padding-left: 0; }
      .bg-demo { height: 120px; border: 1px solid #aaa; background-image: url(assets/logos/hummingbird.svg); background-repeat: no-repeat; background-position: center; background-size: contain; }
      .bg-demo-tile { height: 80px; border: 1px solid #aaa; background-image: url(assets/icons/page_security/secure.png); background-repeat: repeat; background-position: left top; background-size: 16px 16px; }
      .pos-demo { position: relative; height: 90px; border: 1px dashed #999; margin: 8px 0; }
      .pos-base { width: 120px; height: 40px; background-color: #d7e8ff; border: 1px solid #7aa7e3; }
      .pos-overlay { position: absolute; top: 6px; left: 30px; width: 120px; height: 40px; background-color: #ffd3c6; border: 1px solid #e59680; z-index: 2; }
      .box-sizing-demo { width: 160px; padding: 8px; border: 2px solid #666; margin: 4px 0; }
      .box-sizing-content { box-sizing: content-box; background-color: #fff4e6; }
      .box-sizing-border { box-sizing: border-box; background-color: #eaf5ff; }
      .transform-demo { width: 140px; padding: 6px; border: 1px solid #666; background-color: #f8f8f8; }
      .transform-shift { transform: translate(18px, 6px); background-color: #e9f4ff; }
      .opacity-demo-wrap { overflow: hidden; }
      .opacity-demo-card { float: left; width: 180px; margin: 4px 10px 4px 0; padding: 8px; border: 1px solid #666; background-color: #eef3ff; }
      .opacity-demo-100 { opacity: 1; }
      .opacity-demo-65 { opacity: 0.65; }
      .opacity-demo-35 { opacity: 0.35; }
      .opacity-demo-clear { clear: both; }
      .minmax-demo { width: 120px; min-width: 180px; max-width: 200px; min-height: 28px; max-height: 40px; padding: 4px; border: 1px solid #666; background-color: #f0fff0; }
      .baseline-demo { font-size: 16px; }
      .baseline-demo .big { font-size: 28px; }
      .baseline-demo .small { font-size: 12px; }
      .dark-mode-demo { border: 1px solid #bbb; padding: 6px; }
      .dark-mode-column { float: left; width: 320px; margin: 4px 10px 4px 0; }
      .dark-mode-clear { clear: both; }
      .dark-demo-card { border: 1px solid #888; padding: 6px; margin: 4px 0; }
      .dark-demo-card .note { font-size: 13px; }
      .form-demo { display: flex; align-items: center; gap: 6px; }
      .form-demo input { width: 260px; padding: 6px 8px; border: 1px solid #6f6f6f; border-radius: 4px; background-color: #ffffff; }
      .form-demo button, .form-demo-external { padding: 6px 12px; border: 1px solid #5f5f5f; border-radius: 4px; background-color: #ececec; }
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
      <p class="baseline-demo">Baseline <span class="big">BIG</span> <span class="small">small</span> aligned.</p>
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
      <p class="border-radius-demo">Rounded border demo (border-radius: 14px).</p>
      <p class="border-sides-demo">Side-specific border shorthand demo.</p>
      <p class="outline-demo">Outline demo (outline + outline-offset).</p>
    </div>

    <div class="section">
      <h2>Text Readability (CSS)</h2>
      <p class="align-demo">Centered via CSS text-align.</p>
      <p class="nowrap-demo">This sentence should stay on one line even in a narrow box.</p>
      <p class="underline-demo">Underlined via CSS text-decoration.</p>
      <p class="underline-offset-demo">Underline thickness/offset demo.</p>
      <p class="text-transform-demo">text-transform uppercase demo.</p>
      <p class="letter-spacing-demo">letter-spacing demo.</p>
      <p class="text-indent-demo">This paragraph demonstrates first-line text-indent behavior.</p>
      <p class="text-overflow-demo">This long line should render with an ellipsis on the right edge.</p>
      <p class="word-wrap-demo">SupercalifragilisticexpialidociousLongWordWrapSample</p>
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
      <ul class="list-reset">
        <li>Reset list item one</li>
        <li>Reset list item two</li>
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
      <p>50/50 width-hint balancing demo:</p>
      <table class="table-balance-demo" width="100%">
        <tr>
          <td width="50%">Short cell</td>
          <td width="50%">Longer content cell that should still keep a balanced split.</td>
        </tr>
      </table>
    </div>

    <div class="section">
      <h2>Box Sizing</h2>
      <p class="box-sizing-demo box-sizing-content">content-box (total width grows)</p>
      <p class="box-sizing-demo box-sizing-border">border-box (total width fixed)</p>
    </div>

    <div class="section">
      <h2>Transforms</h2>
      <p class="transform-demo">Original position.</p>
      <p class="transform-demo transform-shift">Translated by transform.</p>
    </div>

    <div class="section">
      <h2>Opacity (Paint-Only)</h2>
      <p>These cards keep layout size but use different paint opacity values.</p>
      <div class="opacity-demo-wrap">
        <div class="opacity-demo-card opacity-demo-100"><strong>opacity: 1.0</strong><p>Fully opaque text and background.</p></div>
        <div class="opacity-demo-card opacity-demo-65"><strong>opacity: 0.65</strong><p>Semi-transparent rendering.</p></div>
        <div class="opacity-demo-card opacity-demo-35"><strong>opacity: 0.35</strong><p>More transparent rendering.</p></div>
        <div class="opacity-demo-clear"></div>
      </div>
    </div>

    <div class="section">
      <h2>Min/Max Sizes</h2>
      <p class="minmax-demo">Min/Max size clamp demo.</p>
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
      <h2>Background Images</h2>
      <p>Center/contain example:</p>
      <div class="bg-demo"></div>
      <p>Repeated tiles with explicit size:</p>
      <div class="bg-demo-tile"></div>
    </div>

    <div class="section">
      <h2>Positioning</h2>
      <div class="pos-demo">
        <div class="pos-base">Base block</div>
        <div class="pos-overlay">Absolute overlay</div>
      </div>
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
      <form id="search-form" class="form-demo" action="https://example.dev/search" method="get">
        <input name="q">
        <button type="submit">Search</button>
      </form>
      <p>External submit button:</p>
      <button class="form-demo-external" type="submit" form="search-form">Search (external)</button>
    </div>

    <div class="section">
      <h2>Extension Dark Mode Demo</h2>
      <p>Built-in dark mode now injects global CSS. With the extension enabled, both cards below should render dark.</p>
      <div class="dark-mode-demo">
        <div class="dark-mode-column">
          <div class="dark-demo-card">
            <h3>Global Region A</h3>
            <p>This block should receive dark-mode colors.</p>
            <p class="note">Link sample: <a href="https://example.dev">example.dev</a></p>
            <p class="note"><code>code sample</code> in global region.</p>
          </div>
        </div>
        <div class="dark-mode-column">
          <div class="dark-demo-card">
            <h3>Global Region B</h3>
            <p>This block should also receive dark-mode colors.</p>
            <p class="note">Link sample: <a href="https://example.dev">example.dev</a></p>
            <p class="note"><code>code sample</code> in global region.</p>
          </div>
        </div>
        <div class="dark-mode-clear"></div>
      </div>
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
