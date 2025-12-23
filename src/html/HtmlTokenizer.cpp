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
        if (count >= attrs.size()) {
            break;  // ignore extras
        }
        size_t name_start = m_pos;
        while (!eof() &&
               (std::isalnum(static_cast<unsigned char>(peek_char())) || peek_char() == '-' || peek_char() == '_')) {
            consume_char();
        }
        std::string_view name = m_input.substr(name_start, m_pos - name_start);
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
        attrs[count++] = Attribute{name, value};
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

Token Tokenizer::next_token() {
    while (!eof()) {
        char c = peek_char();
        switch (m_state) {
            case State::Data:
                if (c == '<') {
                    consume_char();
                    if (peek_char() == '/') {
                        consume_char();
                        m_state = State::EndTagOpen;
                    } else {
                        m_state = State::TagOpen;
                    }
                    break;
                }
                return emit_character_data();
            case State::TagOpen: {
                if (peek_char() == '!') {
                    // Skip directives/doctype/comments.
                    if (peek_char(1) == '-' && peek_char(2) == '-') {
                        // Comment: consume until '-->'
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
                    } else {
                        while (!eof() && consume_char() != '>') {
                        }
                    }
                    m_state = State::Data;
                    break;
                }
                if (peek_char() == '?') {
                    while (!eof() && consume_char() != '>') {
                    }
                    m_state = State::Data;
                    break;
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
                return emit_tag(false, self_closing, tag_name, attrs, attr_count);
            }
            case State::EndTagOpen: {
                std::string_view tag_name;
                parse_tag_name(tag_name);
                while (!eof() && peek_char() != '>') consume_char();
                if (peek_char() == '>') consume_char();
                m_state = State::Data;
                return emit_tag(true, false, tag_name, {}, 0);
            }
            default:
                m_state = State::Data;
                break;
        }
    }
    return Token{TokenType::EndOfFile, EndOfFileToken{}};
}

}  // namespace Hummingbird::Html
