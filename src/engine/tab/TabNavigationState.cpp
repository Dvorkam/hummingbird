#include "engine/tab/TabNavigationState.h"

#include <utility>

#include "core/utils/Url.h"

namespace Hummingbird::Engine {

namespace {
SecurityState security_state_for_url(std::string_view url) {
    auto parsed = Core::parse_absolute_url(url);
    if (!parsed) return SecurityState::Unknown;
    if (parsed->scheme == "https") return SecurityState::Secure;
    if (parsed->scheme == "http") return SecurityState::InsecureHttp;
    return SecurityState::Unknown;
}
}  // namespace

void TabNavigationState::begin_navigation(std::string normalized_url, SecurityState initial_state) {
    requested_url_ = std::move(normalized_url);
    security_state_ = initial_state;
    pending_commit_url_.reset();
}

void TabNavigationState::begin_navigation_from_input(std::string_view url) {
    std::string normalized = Core::normalize_input_url(url);
    begin_navigation(std::move(normalized), security_state_for_url(requested_url_));
}

void TabNavigationState::update_requested_url(std::string_view effective_url) {
    if (!effective_url.empty()) {
        requested_url_ = std::string(effective_url);
    }
}

void TabNavigationState::set_pending_commit_url(std::string url) {
    pending_commit_url_ = std::move(url);
}

std::optional<std::string> TabNavigationState::consume_pending_commit_url() {
    if (!pending_commit_url_) {
        return std::nullopt;
    }
    auto url = std::move(pending_commit_url_);
    pending_commit_url_.reset();
    return url;
}

void TabNavigationState::clear_pending_commit_url() {
    pending_commit_url_.reset();
}

}  // namespace Hummingbird::Engine
