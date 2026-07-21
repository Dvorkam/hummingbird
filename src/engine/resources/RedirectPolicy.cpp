#include "engine/resources/RedirectPolicy.h"

#include "core/utils/StringUtils.h"
#include "core/utils/Url.h"

namespace Hummingbird::Engine::RedirectPolicy {

std::optional<Decision> decide(long status, std::string_view location, std::string_view current_url, bool was_post) {
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
    // Never follow a redirect into a pseudo-scheme (javascript:, data:, ...) —
    // that is a redirect-to-script vector, not a navigation.
    if (Core::is_javascript_url(target)) {
        return std::nullopt;
    }

    Decision decision;
    decision.url = std::move(target);
    decision.keep_post = was_post && preserves_method(status);
    return decision;
}

}  // namespace Hummingbird::Engine::RedirectPolicy
