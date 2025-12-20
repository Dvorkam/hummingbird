#include "style/Tokenizer.h"
#include <cctype>

namespace Hummingbird::Css {

Tokenizer::Tokenizer(std::string_view input) : m_input(input) {}

char Tokenizer::peek() const {
    if (m_pos >= m_input.size()) return '\0';
    return m_input[m_pos];
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

std::vector<Token> Tokenizer::tokenize() {
    std::vector<Token> tokens;
    while (!eof()) {
        skip_whitespace();
        if (eof()) break;
        char c = peek();
        switch (c) {
            case '{': tokens.push_back({TokenType::LBrace, "{"}); advance(); break;
            case '}': tokens.push_back({TokenType::RBrace, "}"}); advance(); break;
            case ':': tokens.push_back({TokenType::Colon, ":"}); advance(); break;
            case ';': tokens.push_back({TokenType::Semicolon, ";"}); advance(); break;
            case '.': tokens.push_back({TokenType::Dot, "."}); advance(); break;
            case '#': tokens.push_back({TokenType::Hash, "#"}); advance(); break;
            default:
                if (std::isalpha(static_cast<unsigned char>(c)) || c == '_' || c == '-') {
                    tokens.push_back(identifier());
                } else if (std::isdigit(static_cast<unsigned char>(c))) {
                    tokens.push_back(number());
                } else {
                    // Unknown character; skip it.
                    advance();
                }
                break;
        }
    }
    tokens.push_back({TokenType::End, ""});
    return tokens;
}

} // namespace Hummingbird::Css
