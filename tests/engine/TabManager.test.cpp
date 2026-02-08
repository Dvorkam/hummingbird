#include "engine/tab/TabManager.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/platform_api/ResourceProviderFactory.h"
#include "engine/tab/Tab.h"
#include "test_utils/TestFakes.h"
#include "test_utils/TestGraphicsContext.h"

namespace {
using Hummingbird::Engine::TabId;
using Hummingbird::Engine::TabManager;
using Hummingbird::Test::InlineImageDecoder;
using Hummingbird::Test::InlineNetwork;

class SharedRoutingNetwork final : public Hummingbird::INetwork {
public:
    struct State {
        std::unordered_map<std::string, std::string> responses;
        std::vector<std::string> requested;
    };

    explicit SharedRoutingNetwork(std::shared_ptr<State> state) : state_(std::move(state)) {}

    void get(const std::string& url, std::function<void(Hummingbird::NetworkResponse)> callback,
             const Hummingbird::NetworkRequestOptions& options = {}) override {
        (void)options;
        state_->requested.push_back(url);
        auto it = state_->responses.find(url);
        Hummingbird::NetworkResponse response;
        response.url = url;
        response.effective_url = url;
        if (it != state_->responses.end()) {
            response.status = 200;
            response.body = it->second;
        }
        if (callback) callback(std::move(response));
    }

    void post(const std::string& url, std::string_view body, std::function<void(Hummingbird::NetworkResponse)> callback,
              const Hummingbird::NetworkRequestOptions& options = {}) override {
        (void)body;
        get(url, std::move(callback), options);
    }

    void shutdown() override {}

private:
    std::shared_ptr<State> state_;
};

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

TEST(TabManagerTest, ActivatesNextAndPrev) {
    auto manager = make_manager("<!doctype html><html><body>tab</body></html>");

    const TabId first = manager.create_tab();
    const TabId second = manager.create_tab();
    const TabId third = manager.create_tab();

    EXPECT_EQ(*manager.active_tab_id(), third);

    EXPECT_TRUE(manager.activate_next());
    EXPECT_EQ(*manager.active_tab_id(), first);

    EXPECT_TRUE(manager.activate_prev());
    EXPECT_EQ(*manager.active_tab_id(), third);

    EXPECT_TRUE(manager.set_active(second));
    EXPECT_EQ(*manager.active_tab_id(), second);

    EXPECT_TRUE(manager.activate_prev());
    EXPECT_EQ(*manager.active_tab_id(), first);
}

TEST(TabManagerTest, CloseActiveClosesAndMovesActive) {
    auto manager = make_manager("<!doctype html><html><body>tab</body></html>");

    const TabId first = manager.create_tab();
    const TabId second = manager.create_tab();

    EXPECT_EQ(*manager.active_tab_id(), second);
    EXPECT_TRUE(manager.close_active());
    ASSERT_TRUE(manager.active_tab_id().has_value());
    EXPECT_EQ(*manager.active_tab_id(), first);
    EXPECT_EQ(manager.tab_count(), 1u);
}

TEST(TabManagerTest, TabsDoNotShareResourceStores) {
    auto state = std::make_shared<SharedRoutingNetwork::State>();

    const std::string html_a = R"HTML(
<!doctype html>
<html>
  <head>
    <link rel="stylesheet" href="a.css">
  </head>
  <body>A</body>
</html>
)HTML";
    const std::string html_b = R"HTML(
<!doctype html>
<html>
  <head>
    <link rel="stylesheet" href="b.css">
  </head>
  <body>B</body>
</html>
)HTML";

    state->responses["https://tabs.test/a"] = html_a;
    state->responses["https://tabs.test/a.css"] = "body { color: #111; }";
    state->responses["https://tabs.test/b"] = html_b;
    state->responses["https://tabs.test/b.css"] = "body { color: #222; }";

    Hummingbird::Engine::TabFactory factory;
    factory.create_network = [state]() { return std::make_unique<SharedRoutingNetwork>(state); };
    factory.create_fallback_network = [state]() { return std::make_unique<SharedRoutingNetwork>(state); };
    factory.create_resource_provider = []() { return Hummingbird::create_resource_provider(); };
    factory.create_image_decoder = []() { return Hummingbird::ImageDecoderPtr{}; };
    factory.create_script_engine = []() { return Hummingbird::ScriptEnginePtr{}; };

    TabManager manager(std::move(factory));

    const TabId tab_a_id = manager.create_tab();
    const TabId tab_b_id = manager.create_tab();

    auto* tab_a = manager.tab_by_id(tab_a_id);
    auto* tab_b = manager.tab_by_id(tab_b_id);
    ASSERT_NE(tab_a, nullptr);
    ASSERT_NE(tab_b, nullptr);

    Hummingbird::Test::TestGraphicsContext graphics;
    const Hummingbird::Layout::Rect viewport{0, 0, 800, 600};

    tab_a->navigate("https://tabs.test/a");
    EXPECT_TRUE(tab_a->tick(graphics, viewport));
    EXPECT_TRUE(tab_a->tick(graphics, viewport));

    tab_b->navigate("https://tabs.test/b");
    EXPECT_TRUE(tab_b->tick(graphics, viewport));
    EXPECT_TRUE(tab_b->tick(graphics, viewport));

    auto a_css = tab_a->resource_view("https://tabs.test/a.css", Hummingbird::Engine::ResourceType::Stylesheet);
    ASSERT_TRUE(a_css.has_value());
    EXPECT_EQ(a_css->body, "body { color: #111; }");

    auto b_css = tab_b->resource_view("https://tabs.test/b.css", Hummingbird::Engine::ResourceType::Stylesheet);
    ASSERT_TRUE(b_css.has_value());
    EXPECT_EQ(b_css->body, "body { color: #222; }");

    EXPECT_FALSE(tab_a->resource_view("https://tabs.test/b.css", Hummingbird::Engine::ResourceType::Stylesheet));
    EXPECT_FALSE(tab_b->resource_view("https://tabs.test/a.css", Hummingbird::Engine::ResourceType::Stylesheet));
}
