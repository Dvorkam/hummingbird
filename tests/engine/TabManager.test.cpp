#include "engine/tab/TabManager.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <string>

#include "core/platform_api/ResourceProviderFactory.h"
#include "test_utils/TestFakes.h"

namespace {
using Hummingbird::Engine::TabId;
using Hummingbird::Engine::TabManager;
using Hummingbird::Test::InlineImageDecoder;
using Hummingbird::Test::InlineNetwork;

TabManager make_manager(std::string html) {
    Hummingbird::Engine::TabFactory factory;
    factory.create_network = [html]() { return std::make_unique<InlineNetwork>(html); };
    factory.create_fallback_network = [html]() { return std::make_unique<InlineNetwork>(html); };
    factory.create_resource_provider = []() { return Hummingbird::create_resource_provider(); };
    factory.create_image_decoder = []() { return std::make_unique<InlineImageDecoder>(); };
    factory.create_script_engine = []() { return Hummingbird::ScriptEnginePtr{}; };
    return TabManager(std::move(factory));
}
}  // namespace

TEST(TabManagerTest, CreatesTabsAndTracksActive) {
    auto manager = make_manager("<!doctype html><html><body>tab</body></html>");

    const TabId first = manager.create_tab();
    ASSERT_TRUE(manager.active_tab_id().has_value());
    EXPECT_EQ(*manager.active_tab_id(), first);
    EXPECT_EQ(manager.tab_count(), 1u);

    const TabId second = manager.create_tab();
    EXPECT_EQ(manager.tab_count(), 2u);
    EXPECT_EQ(*manager.active_tab_id(), second);

    EXPECT_TRUE(manager.set_active(first));
    EXPECT_EQ(*manager.active_tab_id(), first);

    auto ids = manager.tab_ids();
    EXPECT_EQ(ids.size(), 2u);
    EXPECT_NE(std::find(ids.begin(), ids.end(), first), ids.end());
    EXPECT_NE(std::find(ids.begin(), ids.end(), second), ids.end());
}

TEST(TabManagerTest, ClosingActiveTabSelectsAnotherOrEmpties) {
    auto manager = make_manager("<!doctype html><html><body>tab</body></html>");

    const TabId first = manager.create_tab();
    const TabId second = manager.create_tab();

    EXPECT_EQ(*manager.active_tab_id(), second);
    EXPECT_TRUE(manager.close_tab(second));
    ASSERT_TRUE(manager.active_tab_id().has_value());
    EXPECT_EQ(*manager.active_tab_id(), first);
    EXPECT_EQ(manager.tab_count(), 1u);

    EXPECT_TRUE(manager.close_tab(first));
    EXPECT_FALSE(manager.active_tab_id().has_value());
    EXPECT_EQ(manager.tab_count(), 0u);
}
