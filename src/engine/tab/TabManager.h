#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include "core/platform_api/IImageDecoder.h"
#include "core/platform_api/INetwork.h"
#include "core/platform_api/IResourceProvider.h"
#include "core/platform_api/IScriptEngine.h"

namespace Hummingbird::Engine {

class Tab;

using TabId = std::uint32_t;

struct TabFactory {
    std::function<NetworkPtr()> create_network;
    std::function<NetworkPtr()> create_fallback_network;
    std::function<ResourceProviderPtr()> create_resource_provider;
    std::function<ImageDecoderPtr()> create_image_decoder;
    std::function<ScriptEnginePtr()> create_script_engine;
};

class TabManager {
public:
    explicit TabManager(TabFactory factory);
    ~TabManager();

    TabManager(const TabManager&) = delete;
    TabManager& operator=(const TabManager&) = delete;
    TabManager(TabManager&&) = delete;
    TabManager& operator=(TabManager&&) = delete;

    TabId create_tab();
    bool close_tab(TabId id);
    bool set_active(TabId id);

    size_t tab_count() const;
    std::vector<TabId> tab_ids() const;
    std::optional<TabId> active_tab_id() const;

    Tab* active_tab();
    const Tab* active_tab() const;
    Tab* tab_by_id(TabId id);
    const Tab* tab_by_id(TabId id) const;

    void shutdown();

private:
    struct Entry {
        TabId id{};
        std::unique_ptr<Tab> tab;
    };

    size_t index_for_id(TabId id) const;

private:
    TabFactory factory_;
    std::vector<Entry> tabs_;
    std::optional<TabId> active_id_;
    TabId next_id_ = 1;
};

}  // namespace Hummingbird::Engine

