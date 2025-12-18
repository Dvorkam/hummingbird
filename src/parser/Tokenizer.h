#pragma once

#include "parser/Token.h"
#include <string_view>

namespace Hummingbird::Parser {

    class Tokenizer {
    public:
        Tokenizer(std::string_view html);

        Token next_token();

    private:
        Token emit_error(std::string_view message);
        Token emit_character_data();
        Token emit_tag();

        char consume_char();
        char peek_char(size_t offset = 0) const;
        bool eof() const;

        std::string_view m_input;
        size_t m_pos = 0;
    };

}
