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
