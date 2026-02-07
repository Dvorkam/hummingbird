#include "platform/graphics/Blend2DFontCache.h"

#include <gtest/gtest.h>

#include <string>

#include "core/utils/AssetPath.h"

namespace {
using Hummingbird::Platform::Blend2DFontCache;
}  // namespace

TEST(Blend2DFontCacheTest, EvictsLeastRecentlyUsedWhenOverLimit) {
    auto& cache = Blend2DFontCache::instance();
    cache.set_max_entries(1);

    const std::string regular = Hummingbird::Core::Utils::resolve_asset_path_string("assets/fonts/Roboto-Regular.ttf");
    const std::string bold = Hummingbird::Core::Utils::resolve_asset_path_string("assets/fonts/Roboto-Bold.ttf");

    ASSERT_NE(cache.get_or_load(regular, 12.0f, true), nullptr);
    EXPECT_TRUE(cache.has_entry(regular, 12.0f));

    ASSERT_NE(cache.get_or_load(bold, 12.0f, true), nullptr);
    EXPECT_EQ(cache.entry_count(), 1u);
    EXPECT_TRUE(cache.has_entry(bold, 12.0f));
    EXPECT_FALSE(cache.has_entry(regular, 12.0f));

    cache.set_max_entries(32);
}
