#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "core/SecurityState.h"

namespace Hummingbird::Engine {

class TabNavigationState {
public:
    void begin_navigation(std::string normalized_url, SecurityState initial_state) {
        requested_url_ = std::move(normalized_url);
        security_state_ = initial_state;
        pending_commit_url_.reset();
    }

    void update_requested_url(std::string_view effective_url) {
        if (!effective_url.empty()) {
            requested_url_ = std::string(effective_url);
        }
    }

    void set_security_state(SecurityState state) { security_state_ = state; }

    std::string_view requested_url() const { return requested_url_; }
    SecurityState security_state() const { return security_state_; }

    void set_pending_commit_url(std::string url) { pending_commit_url_ = std::move(url); }

    std::optional<std::string> consume_pending_commit_url() {
        if (!pending_commit_url_) {
            return std::nullopt;
        }
        auto url = std::move(pending_commit_url_);
        pending_commit_url_.reset();
        return url;
    }

    void clear_pending_commit_url() { pending_commit_url_.reset(); }

private:
    std::string requested_url_;
    SecurityState security_state_ = SecurityState::Unknown;
    std::optional<std::string> pending_commit_url_;
};

}  // namespace Hummingbird::Engine
