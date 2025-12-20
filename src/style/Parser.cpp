#include "style/Parser.h"
#include <sstream>

namespace Hummingbird::Css {

Parser::Parser(std::string_view input) : m_buffer(input) {
    Tokenizer tokenizer(m_buffer);
    m_tokens = tokenizer.tokenize();
}

const Token& Parser::peek() const {
    return m_tokens[m_pos];
}

const Token& Parser::advance() {
    if (!eof()) ++m_pos;
    return m_tokens[m_pos - 1];
}

bool Parser::match(TokenType type) {
    if (peek().type == type) {
        advance();
        return true;
    }
    return false;
}

bool Parser::eof() const {
    return peek().type == TokenType::End;
}

Selector Parser::parse_selector() {
    SelectorType type = SelectorType::Tag;
    if (match(TokenType::Dot)) {
        type = SelectorType::Class;
    } else if (match(TokenType::Hash)) {
        type = SelectorType::Id;
    }
    std::string value;
    if (peek().type == TokenType::Identifier) {
        value = advance().lexeme;
    }
    return Selector{type, value};
}

std::vector<Declaration> Parser::parse_declarations() {
    std::vector<Declaration> decls;
    while (!eof() && peek().type != TokenType::RBrace) {
        if (peek().type != TokenType::Identifier) {
            advance();
            continue;
        }
        std::string property(advance().lexeme);
        if (!match(TokenType::Colon)) {
            continue;
        }
        std::ostringstream value_stream;
        // Collect tokens until semicolon or closing brace.
        while (!eof() && peek().type != TokenType::Semicolon && peek().type != TokenType::RBrace) {
            value_stream << peek().lexeme;
            if (peek().type == TokenType::Identifier || peek().type == TokenType::Number) {
                value_stream << ' ';
            }
            advance();
        }
        std::string value = value_stream.str();
        if (!value.empty() && value.back() == ' ') value.pop_back();
        match(TokenType::Semicolon); // consume if present
        decls.push_back({property, value});
    }
    return decls;
}

Stylesheet Parser::parse() {
    Stylesheet sheet;
    while (!eof()) {
        // Selector
        Selector selector = parse_selector();
        if (!match(TokenType::LBrace)) {
            advance();
            continue;
        }
        auto declarations = parse_declarations();
        match(TokenType::RBrace);
        if (!selector.value.empty()) {
            sheet.rules.push_back({selector, declarations});
        }
    }
    return sheet;
}

} // namespace Hummingbird::Css
