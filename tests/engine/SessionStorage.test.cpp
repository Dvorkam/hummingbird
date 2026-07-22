// Story 8.2.3: window.sessionStorage, driven through a real Tab + real QuickJS
// engine. sessionStorage lives on the Tab (per-tab, never persisted), so it is
// observed here by having the page's script copy what it read into the
// manager-backed localStorage, which the test can inspect.
#include <gtest/gtest.h>

#include <filesystem>
#include <memory>
#include <string>

#include "core/net/Origin.h"
#include "core/net/StorageManager.h"
#include "core/platform_api/ResourceProviderFactory.h"
#include "test_utils/HeadlessTabHarness.h"
#include "test_utils/TestFakes.h"

using Hummingbird::Core::Origin;
using Hummingbird::Core::StorageManager;
using Hummingbird::Test::HeadlessTabHarness;
using Hummingbird::Test::InlineNetwork;
using Hummingbird::Test::RoutingNetwork;

namespace {

class TempStorageDir {
public:
    explicit TempStorageDir(const char* tag)
        : dir_(std::filesystem::temp_directory_path() / (std::string("hb_ss_") + tag)) {
        std::error_code ec;
        std::filesystem::remove_all(dir_, ec);
    }
    ~TempStorageDir() {
        std::error_code ec;
        std::filesystem::remove_all(dir_, ec);
    }
    const std::filesystem::path& path() const { return dir_; }

private:
    std::filesystem::path dir_;
};

std::string page(const std::string& script) {
    return "<!doctype html><html><body><script>" + script + "</script></body></html>";
}

std::string local_value(const std::shared_ptr<StorageManager>& mgr, const char* url, const std::string& key) {
    auto origin = Origin::parse(url);
    if (!origin) return {};
    return mgr->area_for(*origin).get_item(key).value_or(std::string());
}

}  // namespace

TEST(SessionStorageTest, WorksAndIsASeparateNamespaceFromLocalStorage) {
    TempStorageDir dir("sep");
    auto mgr = std::make_shared<StorageManager>(dir.path());
    const char* url = "https://example.dev/page";

    // Writes 's' only into sessionStorage, then copies its readback into the
    // observable localStorage, and records whether localStorage saw 's' too.
    const std::string html = page(
        "sessionStorage.setItem('s', 'hello');"
        "localStorage.setItem('echo', sessionStorage.getItem('s'));"
        "localStorage.setItem('cross', localStorage.getItem('s') === null ? 'isolated' : 'shared');");

    HeadlessTabHarness harness(std::make_unique<InlineNetwork>(html), std::make_unique<InlineNetwork>(html),
                               Hummingbird::create_resource_provider(), nullptr, nullptr, nullptr, mgr);
    harness.set_viewport({0, 0, 800, 600});
    harness.navigate(url);
    harness.tick();
    harness.tick();

    EXPECT_EQ(local_value(mgr, url, "echo"), "hello") << "sessionStorage set/get works";
    EXPECT_EQ(local_value(mgr, url, "cross"), "isolated") << "session and local are different stores";
}

TEST(SessionStorageTest, SurvivesNavigationWithinTheSameTab) {
    TempStorageDir dir("nav");
    auto mgr = std::make_shared<StorageManager>(dir.path());

    auto network = std::make_unique<RoutingNetwork>();
    network->set_response("https://example.dev/a", page("sessionStorage.setItem('k', 'kept');"));
    network->set_response("https://example.dev/b",
                          page("localStorage.setItem('saw', sessionStorage.getItem('k') || 'gone');"));

    HeadlessTabHarness harness(std::move(network), nullptr, Hummingbird::create_resource_provider(), nullptr, nullptr,
                               nullptr, mgr);
    harness.set_viewport({0, 0, 800, 600});

    harness.navigate("https://example.dev/a");
    harness.tick();
    harness.tick();
    // A second navigation in the SAME tab, same origin: the Tab (and its session
    // store) outlives the first document, so the value is still there.
    harness.navigate("https://example.dev/b");
    harness.tick();
    harness.tick();

    EXPECT_EQ(local_value(mgr, "https://example.dev/b", "saw"), "kept");
}

TEST(SessionStorageTest, IsNotSharedBetweenTabs) {
    TempStorageDir dir("tabs");
    auto mgr = std::make_shared<StorageManager>(dir.path());  // shared localStorage, for observation
    const char* url = "https://example.dev/page";

    // Tab 1 writes to sessionStorage and records what it saw.
    {
        const std::string html = page(
            "sessionStorage.setItem('k', 'tab1');"
            "localStorage.setItem('tab1_saw', sessionStorage.getItem('k'));");
        HeadlessTabHarness tab1(std::make_unique<InlineNetwork>(html), std::make_unique<InlineNetwork>(html),
                                Hummingbird::create_resource_provider(), nullptr, nullptr, nullptr, mgr);
        tab1.set_viewport({0, 0, 800, 600});
        tab1.navigate(url);
        tab1.tick();
        tab1.tick();
    }
    // Tab 2, same origin, brand-new tab: its sessionStorage is empty — tab 1's
    // value never leaked, and nothing was persisted.
    {
        const std::string html = page(
            "localStorage.setItem('tab2_saw', sessionStorage.getItem('k') === null ? 'empty' : sessionStorage.getItem('k'));");
        HeadlessTabHarness tab2(std::make_unique<InlineNetwork>(html), std::make_unique<InlineNetwork>(html),
                                Hummingbird::create_resource_provider(), nullptr, nullptr, nullptr, mgr);
        tab2.set_viewport({0, 0, 800, 600});
        tab2.navigate(url);
        tab2.tick();
        tab2.tick();
    }

    EXPECT_EQ(local_value(mgr, url, "tab1_saw"), "tab1");
    EXPECT_EQ(local_value(mgr, url, "tab2_saw"), "empty") << "a second tab sees its own empty sessionStorage";
}
