#include "core/net/StorageManager.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "core/net/Origin.h"
#include "core/net/StorageArea.h"

using Hummingbird::Core::Origin;
using Hummingbird::Core::StorageManager;

namespace {
// A unique temp dir per test, removed on destruction.
class TempStorageDir {
public:
    explicit TempStorageDir(const char* tag)
        : dir_(std::filesystem::temp_directory_path() / (std::string("hb_storage_") + tag)) {
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

Origin origin(const char* url) { return Origin::parse(url).value(); }
}  // namespace

TEST(StorageManagerTest, ValuesSurviveASaveAndReload) {
    TempStorageDir dir("roundtrip");
    {
        StorageManager mgr(dir.path());
        auto& area = mgr.area_for(origin("https://example.dev/"));
        area.set_item("theme", "dark");
        area.set_item("draft", "line one\nline two\twith a tab");  // exercises escaping
        mgr.save_all();
    }

    StorageManager reloaded(dir.path());
    auto& area = reloaded.area_for(origin("https://example.dev/"));
    EXPECT_EQ(area.length(), 2u);
    EXPECT_EQ(area.get_item("theme").value(), "dark");
    // The tab/newline round-trip through escaping intact.
    EXPECT_EQ(area.get_item("draft").value(), "line one\nline two\twith a tab");
}

TEST(StorageManagerTest, OriginsAreIsolatedIncludingBySchemeAndPort) {
    TempStorageDir dir("isolation");
    StorageManager mgr(dir.path());
    mgr.area_for(origin("https://example.dev/")).set_item("k", "https");
    mgr.area_for(origin("http://example.dev/")).set_item("k", "http");
    mgr.area_for(origin("https://example.dev:8443/")).set_item("k", "alt-port");
    mgr.area_for(origin("https://other.dev/")).set_item("k", "other");

    EXPECT_EQ(mgr.area_for(origin("https://example.dev/")).get_item("k").value(), "https");
    EXPECT_EQ(mgr.area_for(origin("http://example.dev/")).get_item("k").value(), "http");
    EXPECT_EQ(mgr.area_for(origin("https://example.dev:8443/")).get_item("k").value(), "alt-port");
    EXPECT_EQ(mgr.area_for(origin("https://other.dev/")).get_item("k").value(), "other");
    EXPECT_EQ(mgr.resident_origins(), 4u);
}

TEST(StorageManagerTest, SameOriginTabsShareOneStore) {
    TempStorageDir dir("shared");
    StorageManager mgr(dir.path());
    // A path difference is still the same origin.
    mgr.area_for(origin("https://example.dev/a")).set_item("k", "v");
    EXPECT_EQ(mgr.area_for(origin("https://example.dev/b/c")).get_item("k").value(), "v");
    EXPECT_EQ(mgr.resident_origins(), 1u);
}

TEST(StorageManagerTest, EachOriginPersistsToItsOwnFile) {
    TempStorageDir dir("perfile");
    {
        StorageManager mgr(dir.path());
        mgr.area_for(origin("https://a.test/")).set_item("k", "1");
        mgr.area_for(origin("https://b.test/")).set_item("k", "2");
        mgr.save_all();
    }
    size_t files = 0;
    for (const auto& entry : std::filesystem::directory_iterator(dir.path())) {
        if (entry.path().extension() == ".tsv") ++files;
    }
    EXPECT_EQ(files, 2u);
}

// Clearing an origin removes its file, so the data is genuinely reclaimed rather
// than lingering as a header-only husk.
TEST(StorageManagerTest, ClearingAnOriginDeletesItsFile) {
    TempStorageDir dir("cleared");
    {
        StorageManager mgr(dir.path());
        mgr.area_for(origin("https://example.dev/")).set_item("k", "v");
        mgr.save_all();
    }
    {
        StorageManager mgr(dir.path());
        mgr.area_for(origin("https://example.dev/")).clear();
        mgr.save_all();
    }
    StorageManager reloaded(dir.path());
    EXPECT_EQ(reloaded.area_for(origin("https://example.dev/")).length(), 0u);
}

TEST(StorageManagerTest, AMissingDirectoryIsANormalFirstRun) {
    TempStorageDir dir("missing");  // constructor removed it
    StorageManager mgr(dir.path());
    EXPECT_EQ(mgr.area_for(origin("https://example.dev/")).length(), 0u);
}

TEST(StorageManagerTest, ACorruptFileStartsThatOriginEmpty) {
    TempStorageDir dir("corrupt");
    std::filesystem::create_directories(dir.path());
    const std::string key = origin("https://example.dev/").key();
    {
        std::ofstream bad(dir.path() / (key + ".tsv"), std::ios::binary);
        bad << "this is not a storage file\n";
    }
    StorageManager mgr(dir.path());
    EXPECT_EQ(mgr.area_for(origin("https://example.dev/")).length(), 0u);
}

TEST(StorageManagerTest, MalformedLinesAreSkippedWithoutLosingGoodOnes) {
    TempStorageDir dir("partial");
    {
        StorageManager mgr(dir.path());
        mgr.area_for(origin("https://example.dev/")).set_item("good", "1");
        mgr.save_all();
    }
    const std::string key = origin("https://example.dev/").key();
    {
        std::ofstream appended(dir.path() / (key + ".tsv"), std::ios::binary | std::ios::app);
        appended << "no-tab-here\n";
        appended << "bad\\escape\t\\q\n";  // invalid escape in the value
    }
    StorageManager mgr(dir.path());
    auto& area = mgr.area_for(origin("https://example.dev/"));
    EXPECT_EQ(area.length(), 1u);
    EXPECT_EQ(area.get_item("good").value(), "1");
}
