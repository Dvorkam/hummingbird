#pragma once

#include <stddef.h>

#include <string>
#include <string_view>
#include <vector>

namespace Hummingbird::Css {

enum class TokenType {
    Whitespace,
    Identifier,
    Number,
    LBrace,
    RBrace,
    Comma,
    Colon,
    Semicolon,
    Dot,
    Hash,
    Star,
    Greater,
    Percent,
    At,
    Url,
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
    char peek_next(size_t offset = 1) const;
    char advance();
    bool eof() const;
    void skip_whitespace();
    Token identifier();
    Token number();
    bool try_url_token(std::vector<Token>& tokens);
    Token emit_single(TokenType type, std::string_view lexeme);
    bool consume_simple_token(std::vector<Token>& tokens);

    std::string_view m_input;
    size_t m_pos = 0;
};

}  // namespace Hummingbird::Css
