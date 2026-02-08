#include "engine/resources/ResourceSecurityPolicy.h"

#include <algorithm>
#include <cctype>

#include "core/utils/Log.h"
#include "core/utils/Url.h"

namespace Hummingbird::Engine {

void ResourceSecurityPolicy::allow_insecure_host(std::string_view host) {
    const std::string normalized = normalize_host(host);
    if (!normalized.empty()) {
        insecure_hosts_.insert(normalized);
        HB_LOG_WARN("[network] allowing insecure TLS for host: " << normalized);
    }
}

bool ResourceSecurityPolicy::is_insecure_allowed_for_url(std::string_view url) const {
    auto parsed = Core::parse_absolute_url(url);
    if (!parsed) return false;
    if (parsed->scheme != "https") return false;
    const std::string host = normalize_host(parsed->host);
    if (host.empty()) return false;
    return insecure_hosts_.find(host) != insecure_hosts_.end();
}

std::string ResourceSecurityPolicy::build_tls_error_body(std::string_view url) {
    std::string body;
    body.reserve(512 + url.size());
    body += R"HTML(<!doctype html>
<html>
  <head>
    <meta charset="utf-8">
    <title>TLS Verification Failed</title>
    <style>
      body { margin: 24px; font-family: sans-serif; color: #222; }
      h1 { margin-bottom: 12px; }
      code { background: #eee; padding: 2px 4px; }
      .hint { margin-top: 16px; }
    </style>
  </head>
  <body>
    <h1>Secure connection failed</h1>
    <p>Hummingbird could not verify the TLS certificate for:</p>
    <p><code>)HTML";
    body.append(url);
    body += R"HTML(</code></p>
    <p class="hint">If you trust this site, click the insecure icon in the URL bar to proceed once.</p>
  </body>
</html>
)HTML";
    return body;
}

std::string ResourceSecurityPolicy::normalize_host(std::string_view host) {
    std::string normalized(host);
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return normalized;
}

}  // namespace Hummingbird::Engine
