#include <gtest/gtest.h>

#include "core/platform_api/ResourceProviderFactory.h"

TEST(ResourceProviderTest, LoadsTextFromAssets) {
    auto provider = Hummingbird::create_resource_provider();
    ASSERT_NE(provider, nullptr);

    auto text = provider->load_text("assets/ua.css");
    ASSERT_TRUE(text.has_value());
    EXPECT_NE(text->find("body"), std::string::npos);
}

TEST(ResourceProviderTest, LoadsBytesFromAssets) {
    auto provider = Hummingbird::create_resource_provider();
    ASSERT_NE(provider, nullptr);

    auto bytes = provider->load_bytes("assets/ua.css");
    ASSERT_TRUE(bytes.has_value());
    EXPECT_NE(bytes->find("body"), std::string::npos);
}
