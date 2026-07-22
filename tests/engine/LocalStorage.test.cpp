// Story 8.2.2: window.localStorage, driven through a real Tab + real QuickJS
// engine + real StorageManager, so the binding, quota throw, origin routing and
// persistence are exercised together.
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

namespace {
class TempStorageDir {
public:
    explicit TempStorageDir(const char* tag)
        : dir_(std::filesystem::temp_directory_path() / (std::string("hb_ls_") + tag)) {
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

// Runs `script` at `url` and returns the resulting store, so a test can assert
// on what JS actually wrote through the real binding.
void run(const std::shared_ptr<StorageManager>& mgr, const std::string& script,
         const char* url = "https://example.dev/page") {
    const std::string html = "<!doctype html><html><body><script>" + script + "</script></body></html>";
    HeadlessTabHarness harness(std::make_unique<InlineNetwork>(html), std::make_unique<InlineNetwork>(html),
                               Hummingbird::create_resource_provider(), nullptr, nullptr, nullptr, mgr);
    harness.set_viewport({0, 0, 800, 600});
    harness.navigate(url);
    harness.tick();
    harness.tick();
}

std::string value(const std::shared_ptr<StorageManager>& mgr, const char* url, const std::string& key) {
    auto origin = Origin::parse(url);
    if (!origin) return {};
    return mgr->area_for(*origin).get_item(key).value_or(std::string());
}
}  // namespace

TEST(LocalStorageTest, SetItemFromScriptIsStored) {
    TempStorageDir dir("set");
    auto mgr = std::make_shared<StorageManager>(dir.path());
    run(mgr, "localStorage.setItem('theme', 'dark');");
    EXPECT_EQ(value(mgr, "https://example.dev/page", "theme"), "dark");
}

TEST(LocalStorageTest, GetItemReadsBackWhatWasStored) {
    TempStorageDir dir("get");
    auto mgr = std::make_shared<StorageManager>(dir.path());
    mgr->area_for(Origin::parse("https://example.dev/").value()).set_item("theme", "dark");
    // The script copies what it read into a second key so the test can see it.
    run(mgr, "localStorage.setItem('echo', localStorage.getItem('theme'));");
    EXPECT_EQ(value(mgr, "https://example.dev/page", "echo"), "dark");
}

TEST(LocalStorageTest, GetItemReturnsNullForAMissingKey) {
    TempStorageDir dir("null");
    auto mgr = std::make_shared<StorageManager>(dir.path());
    run(mgr, "localStorage.setItem('was', localStorage.getItem('absent') === null ? 'null' : 'other');");
    EXPECT_EQ(value(mgr, "https://example.dev/page", "was"), "null");
}

TEST(LocalStorageTest, RemoveItemAndClearAndLength) {
    TempStorageDir dir("mutate");
    auto mgr = std::make_shared<StorageManager>(dir.path());
    run(mgr,
        "localStorage.setItem('a', '1'); localStorage.setItem('b', '2');"
        "localStorage.removeItem('a');"
        "localStorage.setItem('len', String(localStorage.length));");
    EXPECT_EQ(value(mgr, "https://example.dev/page", "b"), "2");
    EXPECT_EQ(value(mgr, "https://example.dev/page", "a"), "");
    // length is read after removing 'a' but before 'len' is inserted, so only
    // 'b' is present at that instant.
    EXPECT_EQ(value(mgr, "https://example.dev/page", "len"), "1");

    run(mgr, "localStorage.clear();");
    EXPECT_EQ(mgr->area_for(Origin::parse("https://example.dev/").value()).length(), 0u);
}

// Non-string arguments are coerced to strings, per the Web Storage spec.
TEST(LocalStorageTest, NonStringValuesAreCoercedToStrings) {
    TempStorageDir dir("coerce");
    auto mgr = std::make_shared<StorageManager>(dir.path());
    run(mgr, "localStorage.setItem('n', 42);");
    EXPECT_EQ(value(mgr, "https://example.dev/page", "n"), "42");
}

// The quota refusal surfaces to JS as a catchable QuotaExceededError, and the
// store is left unchanged.
TEST(LocalStorageTest, ExceedingQuotaThrowsQuotaExceededError) {
    TempStorageDir dir("quota");
    auto mgr = std::make_shared<StorageManager>(dir.path());
    // Pre-fill the origin's store to just under a tiny cap by using a small area
    // directly, then let the script try to overflow it.
    // The default quota is 5 MB, so build a value that exceeds it in one write.
    run(mgr,
        "var big = 'x'.repeat(6 * 1024 * 1024);"
        "try { localStorage.setItem('big', big); localStorage.setItem('outcome', 'stored'); }"
        "catch (e) { localStorage.setItem('outcome', e.name); }");
    EXPECT_EQ(value(mgr, "https://example.dev/page", "outcome"), "QuotaExceededError");
    // The oversized value was not stored.
    EXPECT_EQ(mgr->area_for(Origin::parse("https://example.dev/").value()).get_item("big").has_value(), false);
}

TEST(LocalStorageTest, DifferentOriginsDoNotShareStorage) {
    TempStorageDir dir("origins");
    auto mgr = std::make_shared<StorageManager>(dir.path());
    run(mgr, "localStorage.setItem('k', 'from-dev');", "https://example.dev/");
    run(mgr, "localStorage.setItem('seen', localStorage.getItem('k') === null ? 'isolated' : 'leaked');",
        "https://other.test/");
    EXPECT_EQ(value(mgr, "https://other.test/", "seen"), "isolated");
}

// The whole point of the story: data written by one page load is there for the
// next, and (via the manager) across a restart.
TEST(LocalStorageTest, DataPersistsAcrossPageLoadsAndAReload) {
    TempStorageDir dir("persist");
    {
        auto mgr = std::make_shared<StorageManager>(dir.path());
        run(mgr, "localStorage.setItem('count', '1');");
        mgr->save_all();
    }
    // A fresh manager over the same directory simulates a restart.
    auto reloaded = std::make_shared<StorageManager>(dir.path());
    run(reloaded, "localStorage.setItem('count', String(parseInt(localStorage.getItem('count')) + 1));");
    EXPECT_EQ(value(reloaded, "https://example.dev/page", "count"), "2");
}

// A Tab with no manager (most unit tests) must not crash on localStorage use.
TEST(LocalStorageTest, WithoutAManagerLocalStorageIsInertNotFatal) {
    const std::string html = "<!doctype html><html><body><script>"
                             "localStorage.setItem('a', '1'); var x = localStorage.getItem('a');"
                             "</script></body></html>";
    HeadlessTabHarness harness(std::make_unique<InlineNetwork>(html), std::make_unique<InlineNetwork>(html),
                               Hummingbird::create_resource_provider());
    harness.set_viewport({0, 0, 800, 600});
    harness.navigate("https://example.dev/page");
    harness.tick();
    harness.tick();
    SUCCEED() << "no manager wired up must not crash the script host";
}
