#pragma once

#include <string>
#include <string_view>

#include "core/platform_api/INetwork.h"

namespace Hummingbird::Engine {

// Builds the stable internal document shown when a navigation fails at the
// network layer (DNS/refused/timeout, or a redirect chain that could not
// terminate) instead of leaving a blank page. Story 8.3.2. The page names the
// URL and a human-readable reason, and offers a retry affordance (a link back to
// the same URL, which re-navigates on click; F5 also works).
//
// TLS verification failures have their own page (ResourceSecurityPolicy) because
// they carry a distinct "proceed once if you trust it" affordance.
class NetworkErrorPage {
public:
    static std::string build(std::string_view url, NetworkError error);
};

}  // namespace Hummingbird::Engine
