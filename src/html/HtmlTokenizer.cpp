#include "html/HtmlTokenizer.h"

#include <cctype>

namespace Hummingbird::Html {

Tokenizer::Tokenizer(std::string_view input) : m_input(input) {}

char Tokenizer::peek_char(size_t offset) const {
    if (m_pos + offset >= m_input.length()) {
        return '\0';
    }
    return m_input[m_pos + offset];
}

char Tokenizer::consume_char() {
    if (m_pos >= m_input.length()) return '\0';
    return m_input[m_pos++];
}

bool Tokenizer::eof() const {
    return m_pos >= m_input.length();
}

void Tokenizer::skip_whitespace() {
    while (!eof() && std::isspace(static_cast<unsigned char>(peek_char()))) {
        consume_char();
    }
}

Token Tokenizer::emit_error(std::string_view message) {
    return Token{TokenType::Error, ErrorToken{message}};
}

void Tokenizer::parse_tag_name(std::string_view& out_name) {
    size_t start = m_pos;
    while (!eof() &&
           (std::isalnum(static_cast<unsigned char>(peek_char())) || peek_char() == ':' || peek_char() == '-')) {
        consume_char();
    }
    out_name = m_input.substr(start, m_pos - start);
}

size_t Tokenizer::parse_attributes(std::array<Attribute, 8>& attrs) {
    size_t count = 0;
    while (!eof()) {
        skip_whitespace();
        if (peek_char() == '/' || peek_char() == '>') {
            break;
        }
        size_t name_start = m_pos;
        while (!eof() && (std::isalnum(static_cast<unsigned char>(peek_char())) || peek_char() == '-' ||
                          peek_char() == '_' || peek_char() == ':')) {
            consume_char();
        }
        std::string_view name = m_input.substr(name_start, m_pos - name_start);
        if (name.empty()) {
            consume_char();
            continue;
        }
        skip_whitespace();
        std::string_view value;
        if (peek_char() == '=') {
            consume_char();
            skip_whitespace();
            char quote = '\0';
            if (peek_char() == '"' || peek_char() == '\'') {
                quote = consume_char();
            }
            size_t val_start = m_pos;
            while (!eof() &&
                   ((quote && peek_char() != quote) ||
                    (!quote && !std::isspace(static_cast<unsigned char>(peek_char())) && peek_char() != '>'))) {
                consume_char();
            }
            value = m_input.substr(val_start, m_pos - val_start);
            if (quote && peek_char() == quote) consume_char();
        }
        if (count < attrs.size()) {
            attrs[count++] = Attribute{name, value};
        }
    }
    return count;
}

Token Tokenizer::emit_tag(bool is_end_tag, bool self_closing, std::string_view tag_name,
                          const std::array<Attribute, 8>& attrs, size_t attr_count) {
    if (is_end_tag) {
        return Token{TokenType::EndTag, EndTagToken{tag_name}};
    }
    StartTagToken start;
    start.name = tag_name;
    start.attribute_count = attr_count;
    start.self_closing = self_closing;
    for (size_t i = 0; i < attr_count; ++i) start.attributes[i] = attrs[i];
    return Token{TokenType::StartTag, start};
}

Token Tokenizer::emit_character_data() {
    size_t start = m_pos;
    while (!eof() && peek_char() != '<') {
        consume_char();
    }
    return Token{TokenType::CharacterData, CharacterDataToken{m_input.substr(start, m_pos - start)}};
}

bool Tokenizer::handle_data_state(Token& out) {
    if (peek_char() == '<') {
        consume_char();
        if (peek_char() == '/') {
            consume_char();
            m_state = State::EndTagOpen;
        } else {
            m_state = State::TagOpen;
        }
        return false;
    }
    out = emit_character_data();
    return true;
}

bool Tokenizer::handle_tag_open_state(Token& out) {
    if (peek_char() == '!') {
        skip_directive_or_comment();
        m_state = State::Data;
        return false;
    }
    if (peek_char() == '?') {
        skip_until('>');
        m_state = State::Data;
        return false;
    }
    std::string_view tag_name;
    parse_tag_name(tag_name);
    std::array<Attribute, 8> attrs{};
    size_t attr_count = parse_attributes(attrs);
    bool self_closing = false;
    skip_whitespace();
    if (peek_char() == '/') {
        self_closing = true;
        consume_char();
    }
    if (peek_char() == '>') consume_char();
    m_state = State::Data;
    out = emit_tag(false, self_closing, tag_name, attrs, attr_count);
    return true;
}

bool Tokenizer::handle_end_tag_open_state(Token& out) {
    std::string_view tag_name;
    parse_tag_name(tag_name);
    skip_until('>');
    m_state = State::Data;
    out = emit_tag(true, false, tag_name, {}, 0);
    return true;
}

void Tokenizer::skip_directive_or_comment() {
    if (peek_char(1) == '-' && peek_char(2) == '-') {
        consume_char();  // '!'
        consume_char();  // '-'
        consume_char();  // '-'
        while (!eof()) {
            if (peek_char() == '-' && peek_char(1) == '-' && peek_char(2) == '>') {
                consume_char();
                consume_char();
                consume_char();
                break;
            }
            consume_char();
        }
        return;
    }
    skip_until('>');
}

void Tokenizer::skip_until(char terminal) {
    while (!eof() && consume_char() != terminal) {
    }
}

Token Tokenizer::next_token() {
    while (!eof()) {
        switch (m_state) {
            case State::Data: {
                Token token;
                if (handle_data_state(token)) return token;
                break;
            }
            case State::TagOpen: {
                Token token;
                if (handle_tag_open_state(token)) return token;
                break;
            }
            case State::EndTagOpen: {
                Token token;
                if (handle_end_tag_open_state(token)) return token;
                break;
            }
            default:
                m_state = State::Data;
                break;
        }
    }
    return Token{TokenType::EndOfFile, EndOfFileToken{}};
}

}  // namespace Hummingbird::Html
