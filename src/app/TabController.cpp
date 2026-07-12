#include "app/TabController.h"

#include <utility>

namespace Hummingbird::App {

TabController::TabController(Hummingbird::Engine::TabFactory factory) : tab_manager_(std::move(factory)) {}

size_t TabController::tab_count() const {
    return tab_manager_.tab_count();
}

std::optional<Hummingbird::Engine::TabId> TabController::active_tab_id() const {
    return tab_manager_.active_tab_id();
}

std::vector<Hummingbird::Engine::TabId> TabController::tab_ids() const {
    return tab_manager_.tab_ids();
}

Hummingbird::Engine::Tab* TabController::active_tab() {
    return tab_manager_.active_tab();
}

const Hummingbird::Engine::Tab* TabController::active_tab() const {
    return tab_manager_.active_tab();
}

Hummingbird::Engine::Tab* TabController::tab_by_id(Hummingbird::Engine::TabId id) {
    return tab_manager_.tab_by_id(id);
}

const Hummingbird::Engine::Tab* TabController::tab_by_id(Hummingbird::Engine::TabId id) const {
    return tab_manager_.tab_by_id(id);
}

Hummingbird::Engine::Tab& TabController::ensure_active_tab() {
    auto* tab = tab_manager_.active_tab();
    if (!tab) {
        tab_manager_.create_tab();
        tab = tab_manager_.active_tab();
    }
    return *tab;
}

Hummingbird::Engine::TabId TabController::create_tab() {
    return tab_manager_.create_tab();
}

bool TabController::close_active() {
    return tab_manager_.close_active();
}

bool TabController::activate_next() {
    return tab_manager_.activate_next();
}

bool TabController::activate_prev() {
    return tab_manager_.activate_prev();
}

bool TabController::set_active(Hummingbird::Engine::TabId id) {
    return tab_manager_.set_active(id);
}

void TabController::shutdown() {
    tab_manager_.shutdown();
}

}  // namespace Hummingbird::App
