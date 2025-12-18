#include "parser/Tokenizer.h"
#include <cctype>

namespace Hummingbird::Parser {

Tokenizer::Tokenizer(std::string_view html) : m_input(html) {}

Token Tokenizer::next_token() {
    if (eof()) {
        return Token{TokenType::EndOfFile};
    }

    if (peek_char() == '<') {
        return emit_tag();
    } else {
        return emit_character_data();
    }
}

Token Tokenizer::emit_error(std::string_view message) {
    return Token{TokenType::Error, ErrorToken{message}};
}

Token Tokenizer::emit_character_data() {
    size_t start = m_pos;
    while (!eof() && peek_char() != '<') {
        consume_char();
    }
    return Token{TokenType::CharacterData, CharacterDataToken{m_input.substr(start, m_pos - start)}};
}

Token Tokenizer::emit_tag() {
    consume_char(); // Consume '<'

    bool is_end_tag = false;
    if (peek_char() == '/') {
        is_end_tag = true;
        consume_char(); // Consume '/'
    }

    size_t start = m_pos;
    while (!eof() && std::isalnum(peek_char())) {
        consume_char();
    }
    std::string_view tag_name = m_input.substr(start, m_pos - start);

    if (peek_char() != '>') {
        return emit_error("Unclosed tag");
    }
    consume_char(); // Consume '>'

    if (is_end_tag) {
        return Token{TokenType::EndTag, EndTagToken{tag_name}};
    } else {
        return Token{TokenType::StartTag, StartTagToken{tag_name}};
    }
}

char Tokenizer::consume_char() {
    return m_input[m_pos++];
}

char Tokenizer::peek_char(size_t offset) const {
    if (m_pos + offset >= m_input.length()) {
        return '\0';
    }
    return m_input[m_pos + offset];
}

bool Tokenizer::eof() const {
    return m_pos >= m_input.length();
}

} // namespace Hummingbird::Parser
