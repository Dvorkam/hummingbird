#include <gtest/gtest.h>
#include "html/HtmlParser.h"

using namespace Hummingbird::Html;

TEST(HtmlParserTest, SimpleTreeConstruction) {
    std::string_view html = "<html><body><p>Hello</p></body></html>";
    ArenaAllocator arena(1024);
    Parser parser(arena, html);
    auto dom_tree = parser.parse();
    ASSERT_NE(dom_tree, nullptr);
    ASSERT_EQ(dom_tree->get_children().size(), 1u);
    auto html_node = dom_tree->get_children()[0].get();
    ASSERT_EQ(html_node->get_children().size(), 1u);
    auto body_node = html_node->get_children()[0].get();
    ASSERT_EQ(body_node->get_children().size(), 1u);
}

TEST(HtmlParserTest, CoalescesAdjacentTextNodes) {
    std::string_view html = "<div>Hello <!--comment-->World</div>";
    ArenaAllocator arena(2048);
    Parser parser(arena, html);
    auto dom_tree = parser.parse();
    ASSERT_NE(dom_tree, nullptr);
    ASSERT_EQ(dom_tree->get_children().size(), 1u);
    auto div_node = dynamic_cast<Hummingbird::DOM::Element*>(dom_tree->get_children()[0].get());
    ASSERT_NE(div_node, nullptr);
    ASSERT_EQ(div_node->get_children().size(), 1u);
    auto text_node = dynamic_cast<Hummingbird::DOM::Text*>(div_node->get_children()[0].get());
    ASSERT_NE(text_node, nullptr);
    EXPECT_EQ(text_node->get_text(), "Hello World");
}

TEST(HtmlParserTest, HandlesVoidAndSelfClosingTagsWithoutStackingChildren) {
    std::string_view html = "<div>Hello<br/>World<img src='x'/></div>";
    ArenaAllocator arena(4096);
    Parser parser(arena, html);
    auto dom_tree = parser.parse();
    ASSERT_NE(dom_tree, nullptr);
    auto div_node = dynamic_cast<Hummingbird::DOM::Element*>(dom_tree->get_children()[0].get());
    ASSERT_NE(div_node, nullptr);
    const auto& children = div_node->get_children();
    ASSERT_EQ(children.size(), 3u);
    EXPECT_NE(dynamic_cast<Hummingbird::DOM::Text*>(children[0].get()), nullptr);
    auto br_node = dynamic_cast<Hummingbird::DOM::Element*>(children[1].get());
    ASSERT_NE(br_node, nullptr);
    EXPECT_EQ(br_node->get_tag_name(), "br");
    // The text after the void/self-closing element should still be a sibling.
    auto trailing_text = dynamic_cast<Hummingbird::DOM::Text*>(children[2].get());
    ASSERT_NE(trailing_text, nullptr);
    EXPECT_EQ(trailing_text->get_text(), "World");
}

TEST(HtmlParserTest, TracksUnsupportedTags) {
    std::string_view html = "<custom><inner/></custom><video></video>";
    ArenaAllocator arena(4096);
    Parser parser(arena, html);
    auto dom_tree = parser.parse();
    ASSERT_NE(dom_tree, nullptr);
    const auto& unsupported = parser.unsupported_tags();
    EXPECT_EQ(unsupported.size(), 3u);
    EXPECT_TRUE(unsupported.count("custom"));
    EXPECT_TRUE(unsupported.count("inner"));
    EXPECT_TRUE(unsupported.count("video"));
}
