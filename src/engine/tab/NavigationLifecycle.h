#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "core/SecurityState.h"
#include "engine/tab/TabNavigationState.h"

namespace Hummingbird {
enum class NetworkError;
}

namespace Hummingbird::Engine {

class ResourceLoader;

class NavigationLifecycle {
public:
    void begin_navigation_from_input(std::string_view url);
    // Same-document URL change (fragment navigation): updates the requested URL
    // in place without resetting document/security state (7.2.5 / 7.7.3).
    void update_fragment_url(std::string_view url) { state_.update_requested_url(url); }
    void update_from_document_ready(const ResourceLoader& loader, std::string_view effective_url,
                                    NetworkError document_error);
    void set_pending_commit_url();
    std::optional<std::string> consume_pending_commit_url();
    void clear_pending_commit_url();
    bool allow_insecure_for_current_host(ResourceLoader& loader);

    std::string_view requested_url() const { return state_.requested_url(); }
    SecurityState security_state() const { return state_.security_state(); }

private:
    TabNavigationState state_{};
};

}  // namespace Hummingbird::Engine
