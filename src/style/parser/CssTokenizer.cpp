#include "style/parser/CssTokenizer.h"

#include <cctype>

namespace Hummingbird::Css {

namespace {
bool is_identifier_start(char c) {
    return std::isalpha(static_cast<unsigned char>(c)) || c == '_' || c == '-';
}
}  // namespace

Tokenizer::Tokenizer(std::string_view input) : m_input(input) {}

char Tokenizer::peek() const {
    if (m_pos >= m_input.size()) return '\0';
    return m_input[m_pos];
}

char Tokenizer::peek_next(size_t offset) const {
    size_t index = m_pos + offset;
    if (index >= m_input.size()) return '\0';
    return m_input[index];
}

char Tokenizer::advance() {
    if (m_pos >= m_input.size()) return '\0';
    return m_input[m_pos++];
}

bool Tokenizer::eof() const {
    return m_pos >= m_input.size();
}

void Tokenizer::skip_whitespace() {
    while (!eof() && std::isspace(static_cast<unsigned char>(peek()))) {
        advance();
    }
}

Token Tokenizer::identifier() {
    size_t start = m_pos;
    while (!eof()) {
        char c = peek();
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_') {
            advance();
        } else {
            break;
        }
    }
    return Token{TokenType::Identifier, m_input.substr(start, m_pos - start)};
}

Token Tokenizer::number() {
    size_t start = m_pos;
    if (peek() == '+' || peek() == '-') {
        advance();
    }
    bool seen_dot = false;
    while (!eof()) {
        char c = peek();
        if (std::isdigit(static_cast<unsigned char>(c))) {
            advance();
        } else if (c == '.' && !seen_dot) {
            seen_dot = true;
            advance();
        } else {
            break;
        }
    }
    return Token{TokenType::Number, m_input.substr(start, m_pos - start)};
}

Token Tokenizer::emit_single(TokenType type, std::string_view lexeme) {
    advance();
    return Token{type, lexeme};
}

bool Tokenizer::try_url_token(std::vector<Token>& tokens) {
    if (m_pos + 3 > m_input.size()) {
        return false;
    }
    char c0 = static_cast<char>(std::tolower(static_cast<unsigned char>(m_input[m_pos])));
    char c1 = static_cast<char>(std::tolower(static_cast<unsigned char>(m_input[m_pos + 1])));
    char c2 = static_cast<char>(std::tolower(static_cast<unsigned char>(m_input[m_pos + 2])));
    if (c0 != 'u' || c1 != 'r' || c2 != 'l') {
        return false;
    }
    size_t cursor = m_pos + 3;
    while (cursor < m_input.size() && std::isspace(static_cast<unsigned char>(m_input[cursor]))) {
        ++cursor;
    }
    if (cursor >= m_input.size() || m_input[cursor] != '(') {
        return false;
    }
    ++cursor;
    while (cursor < m_input.size() && std::isspace(static_cast<unsigned char>(m_input[cursor]))) {
        ++cursor;
    }
    size_t content_start = cursor;
    char quote = 0;
    if (cursor < m_input.size() && (m_input[cursor] == '"' || m_input[cursor] == '\'')) {
        quote = m_input[cursor];
        ++content_start;
        cursor = content_start;
        while (cursor < m_input.size() && m_input[cursor] != quote) {
            ++cursor;
        }
        if (cursor >= m_input.size()) {
            return false;
        }
        size_t content_end = cursor;
        ++cursor;
        while (cursor < m_input.size() && std::isspace(static_cast<unsigned char>(m_input[cursor]))) {
            ++cursor;
        }
        if (cursor >= m_input.size() || m_input[cursor] != ')') {
            return false;
        }
        m_pos = cursor + 1;
        tokens.push_back({TokenType::Url, m_input.substr(content_start, content_end - content_start)});
        return true;
    }

    while (cursor < m_input.size() && m_input[cursor] != ')') {
        ++cursor;
    }
    if (cursor >= m_input.size()) {
        return false;
    }
    size_t content_end = cursor;
    while (content_end > content_start && std::isspace(static_cast<unsigned char>(m_input[content_end - 1]))) {
        --content_end;
    }
    m_pos = cursor + 1;
    tokens.push_back({TokenType::Url, m_input.substr(content_start, content_end - content_start)});
    return true;
}

bool Tokenizer::consume_simple_token(std::vector<Token>& tokens) {
    switch (peek()) {
        case '*':
            tokens.push_back(emit_single(TokenType::Star, "*"));
            return true;
        case '{':
            tokens.push_back(emit_single(TokenType::LBrace, "{"));
            return true;
        case '}':
            tokens.push_back(emit_single(TokenType::RBrace, "}"));
            return true;
        case ',':
            tokens.push_back(emit_single(TokenType::Comma, ","));
            return true;
        case ':':
            tokens.push_back(emit_single(TokenType::Colon, ":"));
            return true;
        case ';':
            tokens.push_back(emit_single(TokenType::Semicolon, ";"));
            return true;
        case '.':
            tokens.push_back(emit_single(TokenType::Dot, "."));
            return true;
        case '#':
            tokens.push_back(emit_single(TokenType::Hash, "#"));
            return true;
        case '%':
            tokens.push_back(emit_single(TokenType::Percent, "%"));
            return true;
        case '>':
            tokens.push_back(emit_single(TokenType::Greater, ">"));
            return true;
        case '~':
            tokens.push_back(emit_single(TokenType::Tilde, "~"));
            return true;
        case '+':
            // A '+' starting a number is consumed by the number path first.
            tokens.push_back(emit_single(TokenType::Plus, "+"));
            return true;
        case '@':
            tokens.push_back(emit_single(TokenType::At, "@"));
            return true;
        case '/':
            tokens.push_back(emit_single(TokenType::Slash, "/"));
            return true;
        default:
            return false;
    }
}

std::vector<Token> Tokenizer::tokenize() {
    std::vector<Token> tokens;
    tokens.reserve(m_input.size() + 1);
    while (!eof()) {
        if (std::isspace(static_cast<unsigned char>(peek()))) {
            size_t start = m_pos;
            skip_whitespace();
            tokens.push_back({TokenType::Whitespace, m_input.substr(start, m_pos - start)});
            continue;
        }
        if (try_url_token(tokens)) {
            continue;
        }
        char c = peek();
        char next = peek_next();
        bool starts_number =
            std::isdigit(static_cast<unsigned char>(c)) ||
            (c == '.' && std::isdigit(static_cast<unsigned char>(next))) ||
            ((c == '+' || c == '-') && (std::isdigit(static_cast<unsigned char>(next)) ||
                                        (next == '.' && std::isdigit(static_cast<unsigned char>(peek_next(2))))));
        if (starts_number) {
            tokens.push_back(number());
            continue;
        }
        if (consume_simple_token(tokens)) continue;
        if (is_identifier_start(c)) {
            tokens.push_back(identifier());
        } else {
            // Unknown character; skip it.
            advance();
        }
    }
    tokens.push_back({TokenType::End, ""});
    return tokens;
}

}  // namespace Hummingbird::Css
