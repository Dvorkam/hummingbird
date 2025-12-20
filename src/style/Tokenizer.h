#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace Hummingbird::Css {

enum class TokenType {
    Identifier,
    Number,
    LBrace,
    RBrace,
    Colon,
    Semicolon,
    Dot,
    Hash,
    End,
};

struct Token {
    TokenType type;
    std::string_view lexeme;
};

class Tokenizer {
public:
    explicit Tokenizer(std::string_view input);
    std::vector<Token> tokenize();

private:
    char peek() const;
    char advance();
    bool eof() const;
    void skip_whitespace();
    Token identifier();
    Token number();

    std::string_view m_input;
    size_t m_pos = 0;
};

}  // namespace Hummingbird::Css
