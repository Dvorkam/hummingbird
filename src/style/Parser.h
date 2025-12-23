#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "style/Stylesheet.h"
#include "style/Tokenizer.h"

namespace Hummingbird::Css {

class Parser {
public:
    explicit Parser(std::string_view input);
    Stylesheet parse();

private:
    const Token& peek() const;
    const Token& advance();
    bool match(TokenType type);
    bool eof() const;

    Selector parse_selector();
    std::vector<Selector> parse_selectors();
    std::vector<Declaration> parse_declarations();

    std::string m_buffer;
    std::vector<Token> m_tokens;
    size_t m_pos = 0;
};

}  // namespace Hummingbird::Css
