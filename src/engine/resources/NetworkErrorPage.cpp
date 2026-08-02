#include "engine/resources/NetworkErrorPage.h"

namespace Hummingbird::Engine {

namespace {

// Escapes the URL for safe inclusion in both an attribute and text. The URL is
// the navigation target, not attacker-arbitrary, but a stray quote or angle
// bracket must not break the page or smuggle markup.
std::string escape(std::string_view text) {
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
            case '"':
                out += "&quot;";
                break;
            case '\'':
                out += "&#39;";
                break;
            default:
                out += c;
                break;
        }
    }
    return out;
}

// A short headline + explanation for each failure the page renders.
struct Reason {
    const char* headline;
    const char* detail;
};

Reason reason_for(NetworkError error) {
    switch (error) {
        case NetworkError::TooManyRedirects:
            return {"This page isn&rsquo;t working",
                    "The site redirected too many times, so Hummingbird stopped to avoid looping forever."};
        case NetworkError::RedirectLoop:
            return {"This page isn&rsquo;t working",
                    "The site got stuck redirecting to itself, so the request could not complete."};
        case NetworkError::TlsVerificationFailed:
            return {"Secure connection failed", "Hummingbird could not verify this site&rsquo;s security certificate."};
        case NetworkError::Timeout:
            // Worth its own wording (story 9.1.3): "took too long" tells you to
            // retry, while the generic "didn't respond" reads as "wrong address"
            // and sends you checking the URL instead.
            return {"This page took too long",
                    "The server started responding but did not finish in time, so Hummingbird stopped waiting. It may "
                    "be overloaded &mdash; trying again often works."};
        case NetworkError::CurlError:
        case NetworkError::None:
        default:
            return {"Can&rsquo;t reach this site",
                    "The server didn&rsquo;t respond. It may be offline, or the address may be mistyped or no longer "
                    "exist. Check your connection and try again."};
    }
}

}  // namespace

std::string NetworkErrorPage::build(std::string_view url, NetworkError error) {
    const Reason reason = reason_for(error);
    const std::string safe_url = escape(url);

    std::string body;
    body.reserve(1024 + safe_url.size());
    body += R"HTML(<!doctype html>
<html>
  <head>
    <meta charset="utf-8">
    <title>)HTML";
    body += reason.headline;
    body += R"HTML(</title>
    <style>
      body { margin: 0; background: #f4f6fb; color: #1d2433; font-family: sans-serif; }
      .page { max-width: 560px; margin: 0 auto; padding: 72px 24px; }
      .glyph { font-size: 44px; }
      h1 { color: #14213d; font-size: 24px; margin: 12px 0 10px 0; }
      p { color: #3c4658; line-height: 1.5; }
      .url { background: #e8ecf4; border-radius: 4px; padding: 6px 8px; word-break: break-all; color: #14213d; }
      .retry { display: inline-block; margin-top: 20px; background: #14213d; color: #ffffff; text-decoration: none;
               font-weight: bold; padding: 9px 16px; border-radius: 6px; }
      .hint { color: #6b7688; font-size: 13px; margin-top: 14px; }
    </style>
  </head>
  <body>
    <div class="page">
      <div class="glyph">&#127760;</div>
      <h1>)HTML";
    body += reason.headline;
    body += R"HTML(</h1>
      <p>)HTML";
    body += reason.detail;
    body += R"HTML(</p>
      <p class="url">)HTML";
    body += safe_url;
    body += R"HTML(</p>
      <a class="retry" href=")HTML";
    body += safe_url;
    body += R"HTML(">Try again</a>
      <p class="hint">You can also press F5 to reload.</p>
    </div>
  </body>
</html>
)HTML";
    return body;
}

}  // namespace Hummingbird::Engine
