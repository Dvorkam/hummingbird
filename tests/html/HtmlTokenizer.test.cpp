#include <gtest/gtest.h>
#include "html/HtmlTokenizer.h"

using namespace Hummingbird::Html;

TEST(HtmlTokenizerTest, EmitsTagsAndText) {
    Tokenizer tokenizer("<html>Hello</html>");
    auto t1 = tokenizer.next_token();
    EXPECT_EQ(t1.type, TokenType::StartTag);
    auto start = std::get<StartTagToken>(t1.data);
    EXPECT_EQ(start.name, "html");

    auto t2 = tokenizer.next_token();
    EXPECT_EQ(t2.type, TokenType::CharacterData);
    auto text = std::get<CharacterDataToken>(t2.data);
    EXPECT_EQ(text.data, "Hello");

    auto t3 = tokenizer.next_token();
    EXPECT_EQ(t3.type, TokenType::EndTag);
    auto end = std::get<EndTagToken>(t3.data);
    EXPECT_EQ(end.name, "html");
}

TEST(HtmlTokenizerTest, ParsesAttributes) {
    Tokenizer tokenizer("<div id=\"main\" class='hero'></div>");
    auto t1 = tokenizer.next_token();
    ASSERT_EQ(t1.type, TokenType::StartTag);
    auto start = std::get<StartTagToken>(t1.data);
    EXPECT_EQ(start.name, "div");
    ASSERT_EQ(start.attribute_count, 2u);
    EXPECT_EQ(start.attributes[0].name, "id");
    EXPECT_EQ(start.attributes[0].value, "main");
    EXPECT_EQ(start.attributes[1].name, "class");
    EXPECT_EQ(start.attributes[1].value, "hero");
}
