#include "core/ArenaAllocator.h"

#include <gtest/gtest.h>

#include <new>

TEST(ArenaAllocatorTest, SimpleAllocation) {
    Hummingbird::Core::ArenaAllocator allocator(1024);
    void* ptr = allocator.allocate(100);
    ASSERT_NE(ptr, nullptr);
}

TEST(ArenaAllocatorTest, AllocatesAcrossBlocks) {
    Hummingbird::Core::ArenaAllocator allocator(100);
    allocator.allocate(50);
    void* ptr = allocator.allocate(60);
    ASSERT_NE(ptr, nullptr);
}

TEST(ArenaAllocatorTest, HonorsMaxBlocks) {
    Hummingbird::Core::ArenaAllocator allocator(64, 2);
    allocator.allocate(64);
    allocator.allocate(64);
    EXPECT_THROW(allocator.allocate(64), std::bad_alloc);
}

TEST(ArenaAllocatorTest, Reset) {
    Hummingbird::Core::ArenaAllocator allocator(100);
    void* ptr1 = allocator.allocate(50);
    ASSERT_NE(ptr1, nullptr);

    allocator.reset();

    void* ptr2 = allocator.allocate(70);
    ASSERT_NE(ptr2, nullptr);
    // After reset, we should be able to allocate more than the remaining space before reset
}

TEST(ArenaAllocatorTest, ZeroAllocation) {
    Hummingbird::Core::ArenaAllocator allocator(1024);
    void* ptr = allocator.allocate(0);
    ASSERT_NE(ptr, nullptr);  // Allocating 0 bytes should still return a valid pointer
}

TEST(ArenaAllocatorTest, RespectsAlignment) {
    Hummingbird::Core::ArenaAllocator allocator(1024);
    void* ptr = allocator.allocate(8, 16);
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(reinterpret_cast<size_t>(ptr) % 16u, 0u);
}
