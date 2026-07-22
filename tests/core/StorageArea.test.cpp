#include "core/net/StorageArea.h"

#include <gtest/gtest.h>

#include <string>

using Hummingbird::Core::StorageArea;

TEST(StorageAreaTest, SetGetRemoveClearAndLength) {
    StorageArea area;
    EXPECT_EQ(area.length(), 0u);
    EXPECT_FALSE(area.get_item("missing").has_value());

    EXPECT_TRUE(area.set_item("theme", "dark"));
    EXPECT_TRUE(area.set_item("lang", "en"));
    EXPECT_EQ(area.length(), 2u);
    EXPECT_EQ(area.get_item("theme").value(), "dark");

    area.remove_item("theme");
    EXPECT_EQ(area.length(), 1u);
    EXPECT_FALSE(area.get_item("theme").has_value());

    area.clear();
    EXPECT_EQ(area.length(), 0u);
    EXPECT_FALSE(area.get_item("lang").has_value());
}

TEST(StorageAreaTest, SettingAnExistingKeyOverwritesInPlaceWithoutGrowingLength) {
    StorageArea area;
    area.set_item("k", "one");
    area.set_item("k", "two");
    EXPECT_EQ(area.length(), 1u);
    EXPECT_EQ(area.get_item("k").value(), "two");
}

// getItem must tell a stored empty string apart from an absent key, which a
// plain "" return could not.
TEST(StorageAreaTest, AStoredEmptyStringIsDistinctFromAMissingKey) {
    StorageArea area;
    area.set_item("present", "");
    ASSERT_TRUE(area.get_item("present").has_value());
    EXPECT_EQ(area.get_item("present").value(), "");
    EXPECT_FALSE(area.get_item("absent").has_value());
}

TEST(StorageAreaTest, KeyAtReturnsInsertionOrderAndNulloptWhenOutOfRange) {
    StorageArea area;
    area.set_item("first", "1");
    area.set_item("second", "2");
    EXPECT_EQ(area.key_at(0).value(), "first");
    EXPECT_EQ(area.key_at(1).value(), "second");
    EXPECT_FALSE(area.key_at(2).has_value());

    // Removing the first shifts the rest down, as a 0..length iteration expects.
    area.remove_item("first");
    EXPECT_EQ(area.key_at(0).value(), "second");
}

TEST(StorageAreaTest, RemovingAnAbsentKeyIsANoOp) {
    StorageArea area;
    area.set_item("k", "v");
    area.remove_item("nope");
    EXPECT_EQ(area.length(), 1u);
}

// --- quota -------------------------------------------------------------------

TEST(StorageAreaTest, UsedBytesTracksKeyAndValueLengths) {
    StorageArea area(100);
    area.set_item("ab", "cde");  // 2 + 3
    EXPECT_EQ(area.used_bytes(), 5u);
    area.set_item("ab", "x");  // 2 + 1, replacing
    EXPECT_EQ(area.used_bytes(), 3u);
    area.remove_item("ab");
    EXPECT_EQ(area.used_bytes(), 0u);
}

// An over-quota set is REFUSED, not truncated, and the store is untouched --
// the binding turns the false into a QuotaExceededError.
TEST(StorageAreaTest, AnOverQuotaSetIsRejectedAndLeavesTheStoreUnchanged) {
    StorageArea area(10);
    ASSERT_TRUE(area.set_item("k", "12345"));  // 6 bytes
    EXPECT_FALSE(area.set_item("k2", "12345"));  // would be 6 + 7 = 13 > 10
    EXPECT_EQ(area.length(), 1u);
    EXPECT_FALSE(area.get_item("k2").has_value());
    EXPECT_EQ(area.used_bytes(), 6u);
}

TEST(StorageAreaTest, ExactlyFillingTheQuotaSucceeds) {
    StorageArea area(6);
    EXPECT_TRUE(area.set_item("k", "12345"));  // exactly 6
    EXPECT_EQ(area.used_bytes(), 6u);
}

// Overwriting with a smaller value must never fail on quota, even when the store
// is already full.
TEST(StorageAreaTest, ShrinkingAValueSucceedsAtCapacity) {
    StorageArea area(6);
    ASSERT_TRUE(area.set_item("k", "12345"));  // full
    EXPECT_TRUE(area.set_item("k", "1"));      // net shrink
    EXPECT_EQ(area.used_bytes(), 2u);
    EXPECT_EQ(area.get_item("k").value(), "1");
}

TEST(StorageAreaTest, GrowingAnExistingValueIsCheckedAsTheNetChange) {
    StorageArea area(10);
    ASSERT_TRUE(area.set_item("k", "12"));   // 3 bytes
    EXPECT_TRUE(area.set_item("k", "1234567"));   // 8 bytes, fits in 10
    EXPECT_FALSE(area.set_item("k", "123456789012"));  // 13 bytes, over
    EXPECT_EQ(area.get_item("k").value(), "1234567") << "the rejected grow must not corrupt the value";
}

// --- load (persistence support) ----------------------------------------------

TEST(StorageAreaTest, LoadEntryBypassesTheQuotaSoOnDiskDataIsHonored) {
    // If the cap later shrinks, data already written must still load.
    StorageArea area(4);
    EXPECT_TRUE(area.load_entry("k", "a-long-value-over-quota"));
    EXPECT_EQ(area.get_item("k").value(), "a-long-value-over-quota");
    EXPECT_GT(area.used_bytes(), area.quota_bytes());
}

TEST(StorageAreaTest, LoadEntrySkipsADuplicateKeyRatherThanShadowingIt) {
    StorageArea area;
    EXPECT_TRUE(area.load_entry("k", "first"));
    EXPECT_FALSE(area.load_entry("k", "second"));
    EXPECT_EQ(area.get_item("k").value(), "first");
    EXPECT_EQ(area.length(), 1u);
}

TEST(StorageAreaTest, DefaultQuotaIsFiveMegabytes) {
    StorageArea area;
    EXPECT_EQ(area.quota_bytes(), 5u * 1024 * 1024);
}
