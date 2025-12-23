#include "html/HtmlParser.h"

#include <gtest/gtest.h>

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
    ASSERT_EQ(children.size(), 4u);
    auto leading_text = dynamic_cast<Hummingbird::DOM::Text*>(children[0].get());
    ASSERT_NE(leading_text, nullptr);
    EXPECT_EQ(leading_text->get_text(), "Hello");

    auto br_node = dynamic_cast<Hummingbird::DOM::Element*>(children[1].get());
    ASSERT_NE(br_node, nullptr);
    EXPECT_EQ(br_node->get_tag_name(), "br");

    auto trailing_text = dynamic_cast<Hummingbird::DOM::Text*>(children[2].get());
    ASSERT_NE(trailing_text, nullptr);
    EXPECT_EQ(trailing_text->get_text(), "World");

    auto img_node = dynamic_cast<Hummingbird::DOM::Element*>(children[3].get());
    ASSERT_NE(img_node, nullptr);
    EXPECT_EQ(img_node->get_tag_name(), "img");
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

TEST(HtmlParserTest, PopsToMatchingAncestorOnMismatchedEndTag) {
    // </div> closes both <p> and <span> scopes, then the trailing <p> should attach to root.
    std::string_view html = "<div><span><p>inner</div><p>after</p>";
    ArenaAllocator arena(4096);
    Parser parser(arena, html);
    auto dom_tree = parser.parse();
    ASSERT_NE(dom_tree, nullptr);

    ASSERT_EQ(dom_tree->get_children().size(), 2u);
    auto div_node = dynamic_cast<Hummingbird::DOM::Element*>(dom_tree->get_children()[0].get());
    auto trailing_p = dynamic_cast<Hummingbird::DOM::Element*>(dom_tree->get_children()[1].get());
    ASSERT_NE(div_node, nullptr);
    ASSERT_NE(trailing_p, nullptr);
    EXPECT_EQ(div_node->get_tag_name(), "div");

    ASSERT_EQ(div_node->get_children().size(), 1u);
    auto span_node = dynamic_cast<Hummingbird::DOM::Element*>(div_node->get_children()[0].get());
    ASSERT_NE(span_node, nullptr);
    EXPECT_EQ(span_node->get_tag_name(), "span");

    ASSERT_EQ(span_node->get_children().size(), 1u);
    auto inner_p = dynamic_cast<Hummingbird::DOM::Element*>(span_node->get_children()[0].get());
    ASSERT_NE(inner_p, nullptr);
    EXPECT_EQ(inner_p->get_tag_name(), "p");
    ASSERT_EQ(inner_p->get_children().size(), 1u);
    auto text = dynamic_cast<Hummingbird::DOM::Text*>(inner_p->get_children()[0].get());
    ASSERT_NE(text, nullptr);
    EXPECT_EQ(text->get_text(), "inner");
}

TEST(HtmlParserTest, IsCaseInsensitiveForTags) {
    std::string_view html = "<DIV><A HREF='#'>Link</A></DIV>";
    ArenaAllocator arena(2048);
    Parser parser(arena, html);
    auto dom_tree = parser.parse();
    ASSERT_NE(dom_tree, nullptr);
    auto div_node = dynamic_cast<Hummingbird::DOM::Element*>(dom_tree->get_children()[0].get());
    ASSERT_NE(div_node, nullptr);
    EXPECT_EQ(div_node->get_tag_name(), "div");
    auto a_node = dynamic_cast<Hummingbird::DOM::Element*>(div_node->get_children()[0].get());
    ASSERT_NE(a_node, nullptr);
    EXPECT_EQ(a_node->get_tag_name(), "a");
    auto text_node = dynamic_cast<Hummingbird::DOM::Text*>(a_node->get_children()[0].get());
    ASSERT_NE(text_node, nullptr);
    EXPECT_EQ(text_node->get_text(), "Link");
}

TEST(HtmlParserTest, ExtractsStyleBlocks) {
    std::string_view html = "<style>body { color: red; }</style><p>Hi</p>";
    ArenaAllocator arena(2048);
    Parser parser(arena, html);
    auto dom_tree = parser.parse();
    ASSERT_NE(dom_tree, nullptr);
    const auto& styles = parser.style_blocks();
    ASSERT_EQ(styles.size(), 1u);
    EXPECT_NE(styles[0].find("body"), std::string::npos);
    EXPECT_NE(styles[0].find("color"), std::string::npos);
}

TEST(HtmlParserTest, AutoClosesListItems) {
    std::string_view html = "<ul><li>One<li>Two</ul>";
    ArenaAllocator arena(2048);
    Hummingbird::Html::Parser parser(arena, html);
    auto dom = parser.parse();

    ASSERT_NE(dom, nullptr);
    ASSERT_EQ(dom->get_children().size(), 1u);
    auto* ul = dynamic_cast<Hummingbird::DOM::Element*>(dom->get_children()[0].get());
    ASSERT_NE(ul, nullptr);
    EXPECT_EQ(ul->get_tag_name(), "ul");
    EXPECT_EQ(ul->get_children().size(), 2u);
}
