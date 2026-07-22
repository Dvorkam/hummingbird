#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace Hummingbird::Core {

// Computes the `Referer` header value a request from the document at
// `source_url` should carry when fetching `target_url`, under the browser
// default referrer policy `strict-origin-when-cross-origin`:
//
//   * same origin  -> the full source URL (minus its fragment);
//   * cross origin -> the source origin only ("scheme://host[:port]/");
//   * any TLS downgrade (https source -> http target) -> no header at all;
//   * a source with no tuple origin (empty, or a non-web scheme) -> none.
//
// Returns nullopt when no `Referer` should be sent. `source_url` empty models a
// user-initiated navigation (address bar, bookmark, history), which browsers
// send with no referrer — exactly what nullopt expresses here.
//
// KNOWN GAPS (T-NET-REFERRER-1): userinfo in the source URL is not stripped, and
// a page's own `Referrer-Policy` header / `<meta name="referrer">` / per-link
// `rel="noreferrer"` are not yet honored — the default policy is applied to
// every request.
std::optional<std::string> compute_referrer_header(std::string_view source_url, std::string_view target_url);

}  // namespace Hummingbird::Core
