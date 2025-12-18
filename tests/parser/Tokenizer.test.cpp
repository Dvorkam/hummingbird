#include <gtest/gtest.h>
#include "parser/Tokenizer.h"

using namespace Hummingbird::Parser;

TEST(TokenizerTest, SimpleHTML) {
    std::string_view html = "<div>hi</div>";
    Tokenizer tokenizer(html);

    Token token1 = tokenizer.next_token();
    ASSERT_EQ(token1.type, TokenType::StartTag);
    auto* start_tag = std::get_if<StartTagToken>(&token1.data);
    ASSERT_NE(start_tag, nullptr);
    EXPECT_EQ(start_tag->name, "div");

    Token token2 = tokenizer.next_token();
    ASSERT_EQ(token2.type, TokenType::CharacterData);
    auto* char_data = std::get_if<CharacterDataToken>(&token2.data);
    ASSERT_NE(char_data, nullptr);
    EXPECT_EQ(char_data->data, "hi");

    Token token3 = tokenizer.next_token();
    ASSERT_EQ(token3.type, TokenType::EndTag);
    auto* end_tag = std::get_if<EndTagToken>(&token3.data);
    ASSERT_NE(end_tag, nullptr);
    EXPECT_EQ(end_tag->name, "div");

    Token token4 = tokenizer.next_token();
    ASSERT_EQ(token4.type, TokenType::EndOfFile);
}
