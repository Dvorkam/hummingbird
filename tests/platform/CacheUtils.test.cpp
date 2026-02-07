#include "platform/graphics/CacheUtils.h"

#include <gtest/gtest.h>

#include <unordered_map>

namespace {

struct Entry {
    size_t last_used = 0;
};

}  // namespace

TEST(CacheUtilsTest, FindsOldestEntryByLastUsed) {
    std::unordered_map<int, Entry> entries;
    entries[1] = Entry{5};
    entries[2] = Entry{3};
    entries[3] = Entry{7};

    auto it = Hummingbird::Platform::CacheUtils::find_lru_entry(entries,
                                                                [](const Entry& entry) { return entry.last_used; });
    ASSERT_NE(it, entries.end());
    EXPECT_EQ(it->first, 2);
}

TEST(CacheUtilsTest, ReturnsEndForEmptyMap) {
    std::unordered_map<int, Entry> entries;
    auto it = Hummingbird::Platform::CacheUtils::find_lru_entry(entries,
                                                                [](const Entry& entry) { return entry.last_used; });
    EXPECT_EQ(it, entries.end());
}
