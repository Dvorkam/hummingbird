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

static bool is_selector_start(TokenType type) {
    return type == TokenType::Identifier || type == TokenType::Dot || type == TokenType::Hash;
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

std::vector<Selector> Parser::parse_selectors() {
    std::vector<Selector> selectors;
    if (!is_selector_start(peek().type)) {
        return selectors;
    }
    selectors.push_back(parse_selector());
    while (match(TokenType::Comma)) {
        if (!is_selector_start(peek().type)) {
            break;
        }
        selectors.push_back(parse_selector());
    }
    return selectors;
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
        TokenType prev_type = TokenType::End;
        // Collect tokens until semicolon or closing brace.
        while (!eof() && peek().type != TokenType::Semicolon && peek().type != TokenType::RBrace) {
            const auto& token = peek();
            bool is_value_token = token.type == TokenType::Identifier || token.type == TokenType::Number;
            bool prev_is_value = prev_type == TokenType::Identifier || prev_type == TokenType::Number;
            if (is_value_token && prev_is_value && !(prev_type == TokenType::Number && token.type == TokenType::Identifier)) {
                value_stream << ' ';
            }
            value_stream << token.lexeme;
            prev_type = token.type;
            advance();
        }
        std::string value = value_stream.str();
        if (!value.empty() && value.back() == ' ') value.pop_back();
        match(TokenType::Semicolon);  // consume if present
        decls.push_back({property, value});
    }
    return decls;
}

Stylesheet Parser::parse() {
    Stylesheet sheet;
    while (!eof()) {
        // Selector
        auto selectors = parse_selectors();
        if (!match(TokenType::LBrace)) {
            advance();
            continue;
        }
        auto declarations = parse_declarations();
        match(TokenType::RBrace);
        if (!selectors.empty()) {
            sheet.rules.push_back({selectors, declarations});
        }
    }
    return sheet;
}

}  // namespace Hummingbird::Css
