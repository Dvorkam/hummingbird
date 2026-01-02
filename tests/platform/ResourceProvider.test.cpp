#include "core/platform_api/ResourceProviderFactory.h"

#include <gtest/gtest.h>

TEST(ResourceProviderTest, LoadsTextFromAssets) {
    auto provider = create_resource_provider();
    ASSERT_NE(provider, nullptr);

    auto text = provider->load_text("assets/ua.css");
    ASSERT_TRUE(text.has_value());
    EXPECT_NE(text->find("body"), std::string::npos);
}
