#include "style/Parser.h"

#include <optional>

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

static std::optional<Color> parse_named_color(std::string_view value) {
    if (value == "red") return Color{255, 0, 0, 255};
    if (value == "blue") return Color{0, 0, 255, 255};
    if (value == "black") return Color{0, 0, 0, 255};
    if (value == "white") return Color{255, 255, 255, 255};
    return std::nullopt;
}

static int hex_digit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

static std::optional<Color> parse_hex_color(std::string_view hex) {
    if (hex.size() == 3) {
        int r = hex_digit(hex[0]);
        int g = hex_digit(hex[1]);
        int b = hex_digit(hex[2]);
        if (r < 0 || g < 0 || b < 0) return std::nullopt;
        return Color{static_cast<unsigned char>(r * 17), static_cast<unsigned char>(g * 17),
                     static_cast<unsigned char>(b * 17), 255};
    }
    if (hex.size() == 6) {
        int r1 = hex_digit(hex[0]);
        int r2 = hex_digit(hex[1]);
        int g1 = hex_digit(hex[2]);
        int g2 = hex_digit(hex[3]);
        int b1 = hex_digit(hex[4]);
        int b2 = hex_digit(hex[5]);
        if (r1 < 0 || r2 < 0 || g1 < 0 || g2 < 0 || b1 < 0 || b2 < 0) return std::nullopt;
        return Color{static_cast<unsigned char>((r1 << 4) + r2), static_cast<unsigned char>((g1 << 4) + g2),
                     static_cast<unsigned char>((b1 << 4) + b2), 255};
    }
    return std::nullopt;
}

Property Parser::parse_property() {
    if (peek().type != TokenType::Identifier) return Property::Unknown;
    std::string_view name = advance().lexeme;
    if (name == "display") return Property::Display;
    if (name == "border-width") return Property::BorderWidth;
    if (name == "border-color") return Property::BorderColor;
    if (name == "border-style") return Property::BorderStyle;
    if (name == "margin") return Property::Margin;
    if (name == "margin-top") return Property::MarginTop;
    if (name == "margin-right") return Property::MarginRight;
    if (name == "margin-bottom") return Property::MarginBottom;
    if (name == "margin-left") return Property::MarginLeft;
    if (name == "padding") return Property::Padding;
    if (name == "padding-top") return Property::PaddingTop;
    if (name == "padding-right") return Property::PaddingRight;
    if (name == "padding-bottom") return Property::PaddingBottom;
    if (name == "padding-left") return Property::PaddingLeft;
    if (name == "width") return Property::Width;
    if (name == "height") return Property::Height;
    if (name == "color") return Property::Color;
    if (name == "font-size") return Property::FontSize;
    if (name == "line-height") return Property::LineHeight;
    if (name == "max-width") return Property::MaxWidth;
    return Property::Unknown;
}

Value Parser::parse_value() {
    if (eof()) return Value::identifier("");

    if (match(TokenType::Hash)) {
        if (peek().type == TokenType::Identifier || peek().type == TokenType::Number) {
            std::string hex = std::string(advance().lexeme);
            if (auto color = parse_hex_color(hex)) {
                return Value::color_value(*color);
            }
            return Value::identifier("#" + hex);
        }
        return Value::identifier("#");
    }

    if (peek().type == TokenType::Identifier) {
        std::string ident = std::string(advance().lexeme);
        if (auto color = parse_named_color(ident)) {
            return Value::color_value(*color);
        }
        return Value::identifier(std::move(ident));
    }

    if (peek().type == TokenType::Number) {
        std::string number_text = std::string(advance().lexeme);
        float number = 0.0f;
        try {
            number = std::stof(number_text);
        } catch (...) {
            number = 0.0f;
        }
        if (peek().type == TokenType::Identifier) {
            std::string unit_text = std::string(advance().lexeme);
            Unit unit = unit_text == "px" ? Unit::Px : Unit::Unknown;
            return Value::length_value(number, unit);
        }
        return Value::number_value(number);
    }

    advance();
    return Value::identifier("");
}

std::vector<Declaration> Parser::parse_declarations() {
    std::vector<Declaration> decls;
    while (!eof() && peek().type != TokenType::RBrace) {
        if (peek().type != TokenType::Identifier) {
            advance();
            continue;
        }
        Property property = parse_property();
        if (!match(TokenType::Colon)) {
            continue;
        }
        Value value = parse_value();
        // Skip remaining value tokens we don't model yet.
        while (!eof() && peek().type != TokenType::Semicolon && peek().type != TokenType::RBrace) {
            advance();
        }
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
