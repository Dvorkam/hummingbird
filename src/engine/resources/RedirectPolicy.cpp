#include "engine/resources/RedirectPolicy.h"

#include "core/utils/StringUtils.h"
#include "core/utils/Url.h"

namespace Hummingbird::Engine::RedirectPolicy {

std::string redirect_method(long status, std::string_view current_method) {
    if (status == 303 && current_method != "GET" && current_method != "HEAD") {
        return "GET";
    }
    if ((status == 301 || status == 302) && current_method == "POST") {
        return "GET";
    }
    return std::string(current_method);
}

std::optional<Decision> decide(long status, std::string_view location, std::string_view current_url,
                               std::string_view current_method) {
    if (!is_redirect_status(status)) {
        return std::nullopt;
    }
    location = Core::Utils::trim_ascii_whitespace(location);
    if (location.empty()) {
        // A 3xx with no Location is not actionable; hand it back to the caller.
        return std::nullopt;
    }

    // Location may be relative ("/login") or absolute; resolve either way.
    std::string target = Core::resolve_url(current_url, location);
    if (target.empty() || !Core::parse_absolute_url(target)) {
        return std::nullopt;
    }
    // Only ever follow a redirect to http/https. A pseudo-scheme
    // (javascript:, data:) would be a redirect-to-script vector, and file: /
    // ftp: / smb: would let a server point the engine at the local disk —
    // libcurl serves several of those by default. The transport refuses them
    // too (CURLOPT_PROTOCOLS); this is the engine-side half of that check, so
    // the policy holds for every backend rather than only for curl.
    if (!Core::is_fetchable_web_url(target)) {
        return std::nullopt;
    }

    Decision decision;
    decision.url = std::move(target);
    decision.method = redirect_method(status, current_method);
    return decision;
}

}  // namespace Hummingbird::Engine::RedirectPolicy
