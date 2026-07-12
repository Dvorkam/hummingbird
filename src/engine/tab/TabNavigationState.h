#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "core/SecurityState.h"

namespace Hummingbird::Engine {

class TabNavigationState {
public:
    void begin_navigation(std::string normalized_url, SecurityState initial_state);
    void begin_navigation_from_input(std::string_view url);

    void update_requested_url(std::string_view effective_url);

    void set_security_state(SecurityState state) { security_state_ = state; }

    std::string_view requested_url() const { return requested_url_; }
    SecurityState security_state() const { return security_state_; }

    void set_pending_commit_url(std::string url);

    std::optional<std::string> consume_pending_commit_url();

    void clear_pending_commit_url();

private:
    std::string requested_url_;
    SecurityState security_state_ = SecurityState::Unknown;
    std::optional<std::string> pending_commit_url_;
};

}  // namespace Hummingbird::Engine
