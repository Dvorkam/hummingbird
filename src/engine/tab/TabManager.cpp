#include "engine/tab/TabManager.h"

#include <algorithm>
#include <limits>
#include <utility>

#include "engine/tab/Tab.h"

namespace Hummingbird::Engine {

TabManager::TabManager(TabFactory factory) : factory_(std::move(factory)) {}

TabManager::~TabManager() {
    shutdown();
}

TabId TabManager::create_tab() {
    // TabId 0 is reserved as "invalid".
    if (next_id_ == 0) {
        next_id_ = 1;
    }
    TabId id = next_id_++;
    if (id == 0) {
        id = next_id_++;
    }

    auto tab = std::make_unique<Tab>(factory_.create_network ? factory_.create_network() : nullptr,
                                     factory_.create_fallback_network ? factory_.create_fallback_network() : nullptr,
                                     factory_.create_resource_provider ? factory_.create_resource_provider() : nullptr,
                                     factory_.create_image_decoder ? factory_.create_image_decoder() : nullptr,
                                     factory_.create_script_engine ? factory_.create_script_engine() : nullptr,
                                     cookie_jar_, storage_manager_);

    tabs_.push_back(Entry{id, std::move(tab)});
    active_id_ = id;
    return id;
}

bool TabManager::close_tab(TabId id) {
    const size_t index = index_for_id(id);
    if (index == std::numeric_limits<size_t>::max()) {
        return false;
    }

    if (tabs_[index].tab) {
        tabs_[index].tab->shutdown();
    }

    const bool closing_active = (active_id_.has_value() && *active_id_ == id);
    tabs_.erase(tabs_.begin() + static_cast<std::ptrdiff_t>(index));

    if (tabs_.empty()) {
        active_id_.reset();
        return true;
    }

    if (closing_active) {
        // Prefer keeping the same index if possible (next tab slides into this position),
        // otherwise fall back to the last entry.
        const size_t new_index = std::min(index, tabs_.size() - 1);
        active_id_ = tabs_[new_index].id;
    }
    return true;
}

bool TabManager::set_active(TabId id) {
    const size_t index = index_for_id(id);
    if (index == std::numeric_limits<size_t>::max()) {
        return false;
    }
    active_id_ = tabs_[index].id;
    return true;
}

bool TabManager::activate_next() {
    if (tabs_.empty() || !active_id_) return false;
    const size_t index = index_for_id(*active_id_);
    if (index == std::numeric_limits<size_t>::max()) return false;
    const size_t next = (index + 1) % tabs_.size();
    active_id_ = tabs_[next].id;
    return true;
}

bool TabManager::activate_prev() {
    if (tabs_.empty() || !active_id_) return false;
    const size_t index = index_for_id(*active_id_);
    if (index == std::numeric_limits<size_t>::max()) return false;
    const size_t prev = (index + tabs_.size() - 1) % tabs_.size();
    active_id_ = tabs_[prev].id;
    return true;
}

bool TabManager::close_active() {
    if (!active_id_) return false;
    return close_tab(*active_id_);
}

size_t TabManager::tab_count() const {
    return tabs_.size();
}

std::vector<TabId> TabManager::tab_ids() const {
    std::vector<TabId> ids;
    ids.reserve(tabs_.size());
    for (const auto& entry : tabs_) {
        ids.push_back(entry.id);
    }
    return ids;
}

std::optional<TabId> TabManager::active_tab_id() const {
    return active_id_;
}

Tab* TabManager::active_tab() {
    if (!active_id_) return nullptr;
    return tab_by_id(*active_id_);
}

const Tab* TabManager::active_tab() const {
    if (!active_id_) return nullptr;
    return tab_by_id(*active_id_);
}

Tab* TabManager::tab_by_id(TabId id) {
    const size_t index = index_for_id(id);
    if (index == std::numeric_limits<size_t>::max()) {
        return nullptr;
    }
    return tabs_[index].tab.get();
}

const Tab* TabManager::tab_by_id(TabId id) const {
    const size_t index = index_for_id(id);
    if (index == std::numeric_limits<size_t>::max()) {
        return nullptr;
    }
    return tabs_[index].tab.get();
}

void TabManager::shutdown() {
    for (auto& entry : tabs_) {
        if (entry.tab) {
            entry.tab->shutdown();
        }
    }
    tabs_.clear();
    active_id_.reset();
}

size_t TabManager::index_for_id(TabId id) const {
    const auto it = std::find_if(tabs_.begin(), tabs_.end(), [&](const Entry& entry) { return entry.id == id; });
    if (it == tabs_.end()) {
        return std::numeric_limits<size_t>::max();
    }
    return static_cast<size_t>(std::distance(tabs_.begin(), it));
}

}  // namespace Hummingbird::Engine
