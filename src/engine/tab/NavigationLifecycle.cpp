#include "engine/tab/NavigationLifecycle.h"

#include "core/utils/Url.h"
#include "engine/resources/ResourceLoader.h"
#include "engine/tab/TabDocumentReadyPolicy.h"

namespace Hummingbird::Engine {

void NavigationLifecycle::begin_navigation_from_input(std::string_view url) {
    state_.begin_navigation_from_input(url);
}

void NavigationLifecycle::update_from_document_ready(const ResourceLoader& loader, std::string_view effective_url,
                                                     NetworkError document_error) {
    state_.update_requested_url(effective_url);
    state_.set_security_state(
        TabDocumentReadyPolicy::decide_security_state(loader, state_.requested_url(), document_error));
}

void NavigationLifecycle::set_pending_commit_url() {
    state_.set_pending_commit_url(std::string(state_.requested_url()));
}

std::optional<std::string> NavigationLifecycle::consume_pending_commit_url() {
    return state_.consume_pending_commit_url();
}

void NavigationLifecycle::clear_pending_commit_url() {
    state_.clear_pending_commit_url();
}

bool NavigationLifecycle::allow_insecure_for_current_host(ResourceLoader& loader) {
    auto parsed = Core::parse_absolute_url(state_.requested_url());
    if (!parsed || parsed->host.empty()) {
        return false;
    }
    loader.allow_insecure_host(parsed->host);
    state_.set_security_state(SecurityState::InsecureTls);
    return true;
}

}  // namespace Hummingbird::Engine
