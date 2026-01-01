#include "style/StylesheetSource.h"

#include <gtest/gtest.h>

TEST(StylesheetSourceTest, MergesSourcesInOrder) {
    std::string ua = "body { color: red; }";
    std::vector<std::string> links = {"p { color: blue; }", "p { color: orange; }"};
    std::vector<std::string> blocks = {"p { color: green; }"};

    auto merged = Hummingbird::Css::merge_css_sources(ua, links, blocks);

    auto pos_ua = merged.find("body { color: red; }");
    auto pos_link1 = merged.find("p { color: blue; }");
    auto pos_link2 = merged.find("p { color: orange; }");
    auto pos_block = merged.find("p { color: green; }");

    ASSERT_NE(pos_ua, std::string::npos);
    ASSERT_NE(pos_link1, std::string::npos);
    ASSERT_NE(pos_link2, std::string::npos);
    ASSERT_NE(pos_block, std::string::npos);

    EXPECT_LT(pos_ua, pos_link1);
    EXPECT_LT(pos_link1, pos_link2);
    EXPECT_LT(pos_link2, pos_block);
}
