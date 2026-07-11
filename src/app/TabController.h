#pragma once

#include <optional>
#include <vector>

#include "engine/tab/TabManager.h"

namespace Hummingbird::App {

class TabController {
public:
    explicit TabController(Hummingbird::Engine::TabFactory factory);

    size_t tab_count() const;
    std::optional<Hummingbird::Engine::TabId> active_tab_id() const;
    std::vector<Hummingbird::Engine::TabId> tab_ids() const;

    Hummingbird::Engine::Tab* active_tab();
    const Hummingbird::Engine::Tab* active_tab() const;
    // Returns the active tab, creating one first if none exists. The app relies on
    // an active tab always being available.
    Hummingbird::Engine::Tab& ensure_active_tab();
    Hummingbird::Engine::Tab* tab_by_id(Hummingbird::Engine::TabId id);
    const Hummingbird::Engine::Tab* tab_by_id(Hummingbird::Engine::TabId id) const;

    Hummingbird::Engine::TabId create_tab();
    bool close_active();
    bool activate_next();
    bool activate_prev();
    bool set_active(Hummingbird::Engine::TabId id);

    void shutdown();

    const Hummingbird::Engine::TabManager& manager() const { return tab_manager_; }

private:
    Hummingbird::Engine::TabManager tab_manager_;
};

}  // namespace Hummingbird::App
