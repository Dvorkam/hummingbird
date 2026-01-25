#include "style/CssParser.h"

#include <optional>
#include <ostream>
#include <utility>

#include "core/platform_api/IGraphicsContext.h"
#include "core/utils/Log.h"
#include "style/CssPropertyRegistry.h"
#include "style/CssValueNames.h"

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
    Selector selector;
    if (peek().type == TokenType::Identifier) {
        selector.tag = advance().lexeme;
    }
    while (true) {
        if (match(TokenType::Dot)) {
            if (peek().type == TokenType::Identifier) {
                selector.classes.emplace_back(advance().lexeme);
                continue;
            }
            break;
        }
        if (match(TokenType::Hash)) {
            if (peek().type == TokenType::Identifier) {
                selector.id = advance().lexeme;
                continue;
            }
            break;
        }
        break;
    }
    return selector;
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
    if (value == ValueNames::Red) return Color{255, 0, 0, 255};
    if (value == ValueNames::Blue) return Color{0, 0, 255, 255};
    if (value == ValueNames::Black) return Color{0, 0, 0, 255};
    if (value == ValueNames::White) return Color{255, 255, 255, 255};
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
    return parse_property_name(advance().lexeme);
}

Value Parser::parse_value() {
    if (eof()) return Value::identifier("");

    if (match(TokenType::Hash)) return parse_hash_value();
    if (peek().type == TokenType::Identifier) return parse_identifier_value();
    if (peek().type == TokenType::Number) return parse_number_value();

    advance();
    return Value::identifier("");
}

std::vector<Value> Parser::parse_value_list() {
    std::vector<Value> values;
    while (!eof() && peek().type != TokenType::Semicolon && peek().type != TokenType::RBrace) {
        if (peek().type == TokenType::Hash || peek().type == TokenType::Identifier ||
            peek().type == TokenType::Number) {
            values.push_back(parse_value());
            continue;
        }
        advance();
    }
    return values;
}

std::vector<Declaration> Parser::parse_declarations() {
    std::vector<Declaration> decls;
    while (!eof() && peek().type != TokenType::RBrace) {
        if (peek().type != TokenType::Identifier) {
            advance();
            continue;
        }
        consume_declaration(decls);
    }
    return decls;
}

Property Parser::parse_property_name(std::string_view name) const {
    return PropertyRegistry::parse_property_name(name);
}

Value Parser::parse_hash_value() {
    if (peek().type == TokenType::Identifier || peek().type == TokenType::Number) {
        std::string hex;
        while (peek().type == TokenType::Identifier || peek().type == TokenType::Number) {
            hex.append(advance().lexeme);
        }
        if (auto color = parse_hex_color(hex)) {
            return Value::color_value(*color);
        }
        return Value::identifier("#" + hex);
    }
    return Value::identifier("#");
}

Value Parser::parse_identifier_value() {
    std::string ident = std::string(advance().lexeme);
    if (auto color = parse_named_color(ident)) {
        return Value::color_value(*color);
    }
    return Value::identifier(std::move(ident));
}

Value Parser::parse_number_value() {
    std::string number_text = std::string(advance().lexeme);
    float number = 0.0f;
    try {
        number = std::stof(number_text);
    } catch (...) {
        number = 0.0f;
    }
    if (peek().type == TokenType::Identifier) {
        std::string unit_text = std::string(advance().lexeme);
        Unit unit = unit_text == ValueNames::Px ? Unit::Px : Unit::Unknown;
        return Value::length_value(number, unit);
    }
    return Value::number_value(number);
}

bool Parser::consume_declaration(std::vector<Declaration>& decls) {
    std::string_view property_name;
    if (peek().type == TokenType::Identifier) {
        property_name = peek().lexeme;
    }
    Property property = parse_property();
    if (!match(TokenType::Colon)) {
        return false;
    }
    std::vector<Value> values = parse_value_list();
    match(TokenType::Semicolon);  // consume if present

    if (property == Property::Unknown && !property_name.empty()) {
        std::string name(property_name);
        if (m_unknown_properties.insert(name).second) {
            HB_LOG_WARN("[parser] Unsupported CSS property encountered: " << name);
        }
    }

    auto emit_edges = [&](Property top, Property right, Property bottom, Property left) {
        if (values.empty()) {
            return;
        }
        const size_t count = values.size();
        const Value& top_value = values[0];
        const Value& right_value = count > 1 ? values[1] : values[0];
        const Value& bottom_value = count > 2 ? values[2] : values[0];
        const Value& left_value = count > 3 ? values[3] : (count > 1 ? values[1] : values[0]);
        decls.push_back({top, top_value});
        decls.push_back({right, right_value});
        decls.push_back({bottom, bottom_value});
        decls.push_back({left, left_value});
    };

    if (property == Property::Margin) {
        emit_edges(Property::MarginTop, Property::MarginRight, Property::MarginBottom, Property::MarginLeft);
        return true;
    }
    if (property == Property::Padding) {
        emit_edges(Property::PaddingTop, Property::PaddingRight, Property::PaddingBottom, Property::PaddingLeft);
        return true;
    }
    if (property == Property::Border) {
        std::optional<Value> border_width;
        std::optional<Value> border_style;
        std::optional<Value> border_color;
        for (const auto& value : values) {
            if (!border_width && value.type == Value::Type::Length) {
                border_width = value;
                continue;
            }
            if (!border_color && value.type == Value::Type::Color) {
                border_color = value;
                continue;
            }
            if (!border_style && value.type == Value::Type::Identifier && value.ident == ValueNames::Solid) {
                border_style = value;
            }
        }
        if (border_width) decls.push_back({Property::BorderWidth, *border_width});
        if (border_style) decls.push_back({Property::BorderStyle, *border_style});
        if (border_color) decls.push_back({Property::BorderColor, *border_color});
        return true;
    }
    if (property == Property::Background) {
        for (const auto& value : values) {
            if (value.type == Value::Type::Color) {
                decls.push_back({Property::BackgroundColor, value});
                break;
            }
        }
        return true;
    }

    Value value = values.empty() ? Value::identifier("") : values.front();
    decls.push_back({property, value});
    return true;
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
