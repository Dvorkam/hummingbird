#pragma once

#include "style/Stylesheet.h"
#include "style/Tokenizer.h"
#include <vector>
#include <string_view>
#include <string>

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
        std::vector<Declaration> parse_declarations();

        std::string m_buffer;
        std::vector<Token> m_tokens;
        size_t m_pos = 0;
    };

} // namespace Hummingbird::Css
