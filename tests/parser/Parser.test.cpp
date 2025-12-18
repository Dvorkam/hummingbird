#include <gtest/gtest.h>
#include "parser/Parser.h"
#include "core/dom/Element.h"
#include "core/dom/Text.h"

using namespace Hummingbird::Parser;
using namespace Hummingbird::DOM;

TEST(ParserTest, SimpleTreeConstruction) {
    std::string_view html = "<html><body><p>Test</p></body></html>";
    Parser parser(html);
    std::unique_ptr<Node> root = parser.parse();

    ASSERT_NE(root, nullptr);
    ASSERT_EQ(root->get_children().size(), 1);

    const auto* html_node = dynamic_cast<const Element*>(root->get_children()[0].get());
    ASSERT_NE(html_node, nullptr);
    EXPECT_EQ(html_node->get_tag_name(), "html");
    ASSERT_EQ(html_node->get_children().size(), 1);

    const auto* body_node = dynamic_cast<const Element*>(html_node->get_children()[0].get());
    ASSERT_NE(body_node, nullptr);
    EXPECT_EQ(body_node->get_tag_name(), "body");
    ASSERT_EQ(body_node->get_children().size(), 1);

    const auto* p_node = dynamic_cast<const Element*>(body_node->get_children()[0].get());
    ASSERT_NE(p_node, nullptr);
    EXPECT_EQ(p_node->get_tag_name(), "p");
    ASSERT_EQ(p_node->get_children().size(), 1);

    const auto* text_node = dynamic_cast<const Text*>(p_node->get_children()[0].get());
    ASSERT_NE(text_node, nullptr);
    EXPECT_EQ(text_node->get_text(), "Test");
}
