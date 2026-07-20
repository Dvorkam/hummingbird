#include "core/BookmarkStore.h"

#include <gtest/gtest.h>

#include <atomic>
#include <filesystem>
#include <string>

namespace {
using Hummingbird::Core::BookmarkStore;

// A fresh, non-existent temp path per call so each test starts unseeded.
std::filesystem::path fresh_temp_path() {
    static std::atomic<int> counter{0};
    auto path =
        std::filesystem::temp_directory_path() / ("hb_bookmarks_" + std::to_string(counter.fetch_add(1)) + ".tsv");
    std::error_code ec;
    std::filesystem::remove(path, ec);
    return path;
}

const Hummingbird::Core::Bookmark* find(const BookmarkStore& store, const std::string& url) {
    for (const auto& entry : store.entries()) {
        if (entry.url == url) return &entry;
    }
    return nullptr;
}
}  // namespace

TEST(BookmarkStoreTest, SeedsWithReferencePageWhenFileMissing) {
    BookmarkStore store(fresh_temp_path());
    ASSERT_EQ(store.entries().size(), 1u);
    EXPECT_NE(store.entries()[0].url.find("duckduckgo"), std::string::npos);
}

TEST(BookmarkStoreTest, AddDedupesByUrlAndRefreshesTitle) {
    BookmarkStore store(fresh_temp_path());  // seeded with DuckDuckGo
    EXPECT_TRUE(store.add("https://a.test/", "A"));
    EXPECT_FALSE(store.add("https://a.test/", "A refreshed"));  // same URL -> not new
    EXPECT_TRUE(store.add("https://b.test/", "B"));

    ASSERT_EQ(store.entries().size(), 3u);  // seed + A + B
    const auto* a = find(store, "https://a.test/");
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(a->title, "A refreshed");  // title updated in place by the dedupe
}

TEST(BookmarkStoreTest, PersistsAcrossReload) {
    const auto path = fresh_temp_path();
    {
        BookmarkStore store(path);
        EXPECT_TRUE(store.add("https://a.test/", "Page A"));
        store.save();
    }
    BookmarkStore reloaded(path);
    const auto* a = find(reloaded, "https://a.test/");
    ASSERT_NE(a, nullptr) << "bookmark did not survive a reload";
    EXPECT_EQ(a->title, "Page A");
    // The seed is persisted on first save, so it survives too.
    EXPECT_GE(reloaded.entries().size(), 2u);
    // A reload of an existing file does not re-seed duplicates.
    EXPECT_EQ(reloaded.entries().size(), 2u);
}

TEST(BookmarkStoreTest, RenderHtmlEscapesLinks) {
    BookmarkStore store(fresh_temp_path());
    store.add("https://a.test/x?y=1&z=2", "A & B <ok>");
    const std::string html = store.render_html();
    EXPECT_NE(html.find("href=\"https://a.test/x?y=1&amp;z=2\""), std::string::npos) << html;
    EXPECT_NE(html.find("A &amp; B &lt;ok&gt;"), std::string::npos) << html;
}
