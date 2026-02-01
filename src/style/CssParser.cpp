#include "style/CssParser.h"

#include <optional>
#include <ostream>
#include <utility>

#include "core/platform_api/IGraphicsContext.h"
#include "core/utils/ColorUtils.h"
#include "core/utils/Log.h"
#include "core/utils/StringUtils.h"
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

void Parser::skip_whitespace_tokens() {
    while (peek().type == TokenType::Whitespace) {
        advance();
    }
}

static bool is_selector_start(TokenType type) {
    return type == TokenType::Identifier || type == TokenType::Dot || type == TokenType::Hash ||
           type == TokenType::Star;
}

Selector Parser::parse_selector() {
    Selector selector;
    skip_whitespace_tokens();
    while (is_selector_start(peek().type)) {
        selector.parts.push_back(parse_simple_selector());
        bool saw_whitespace = false;
        while (peek().type == TokenType::Whitespace) {
            saw_whitespace = true;
            advance();
        }
        if (!saw_whitespace) {
            break;
        }
    }
    return selector;
}

SelectorPart Parser::parse_simple_selector() {
    SelectorPart selector;
    if (match(TokenType::Star)) {
        selector.tag = "*";
    } else if (peek().type == TokenType::Identifier) {
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
    skip_whitespace_tokens();
    if (!is_selector_start(peek().type)) {
        return selectors;
    }
    selectors.push_back(parse_selector());
    skip_whitespace_tokens();
    while (match(TokenType::Comma)) {
        skip_whitespace_tokens();
        if (!is_selector_start(peek().type)) {
            break;
        }
        selectors.push_back(parse_selector());
        skip_whitespace_tokens();
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

Property Parser::parse_property() {
    if (peek().type != TokenType::Identifier) return Property::Unknown;
    return parse_property_name(advance().lexeme);
}

Value Parser::parse_value() {
    if (eof()) return Value::identifier("");

    if (match(TokenType::Hash)) return parse_hash_value();
    if (peek().type == TokenType::Url) {
        return Value::url_value(std::string(advance().lexeme));
    }
    if (peek().type == TokenType::Identifier) return parse_identifier_value();
    if (peek().type == TokenType::Number) return parse_number_value();

    advance();
    return Value::identifier("");
}

std::vector<Value> Parser::parse_value_list() {
    std::vector<Value> values;
    while (!eof() && peek().type != TokenType::Semicolon && peek().type != TokenType::RBrace) {
        if (peek().type == TokenType::Whitespace) {
            advance();
            continue;
        }
        if (peek().type == TokenType::Identifier) {
            size_t lookahead = m_pos + 1;
            while (lookahead < m_tokens.size() && m_tokens[lookahead].type == TokenType::Whitespace) {
                ++lookahead;
            }
            if (lookahead < m_tokens.size() && m_tokens[lookahead].type == TokenType::Colon) {
                break;
            }
        }
        if (peek().type == TokenType::Hash || peek().type == TokenType::Identifier ||
            peek().type == TokenType::Number || peek().type == TokenType::Url) {
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
        if (auto color = Core::Utils::parse_hex_color(hex)) {
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
        Unit unit = Unit::Unknown;
        if (unit_text == ValueNames::Px) {
            unit = Unit::Px;
        } else if (unit_text == ValueNames::Em) {
            unit = Unit::Em;
        }
        return Value::length_value(number, unit);
    }
    return Value::number_value(number);
}

std::string Parser::parse_font_family_list() {
    std::string list;
    std::string current;

    auto push_current = [&]() {
        if (current.empty()) return;
        if (!list.empty()) {
            list.push_back(',');
        }
        list += current;
        current.clear();
    };

    while (!eof() && peek().type != TokenType::Semicolon && peek().type != TokenType::RBrace) {
        if (peek().type == TokenType::Whitespace) {
            advance();
            continue;
        }
        if (peek().type == TokenType::Comma) {
            advance();
            push_current();
            continue;
        }
        if (peek().type == TokenType::Identifier) {
            std::string ident = Core::Utils::to_lower(advance().lexeme);
            if (!current.empty()) {
                current.push_back(' ');
            }
            current += ident;
            continue;
        }
        advance();
    }
    push_current();
    return list;
}

std::string Parser::parse_custom_property_value() {
    std::string raw;
    while (!eof() && peek().type != TokenType::Semicolon && peek().type != TokenType::RBrace) {
        raw.append(advance().lexeme);
    }
    auto trimmed = Core::Utils::trim_ascii_whitespace(raw);
    return std::string(trimmed);
}

bool Parser::consume_declaration(std::vector<Declaration>& decls) {
    std::string_view property_name;
    if (peek().type == TokenType::Identifier) {
        property_name = peek().lexeme;
    }
    Property property = parse_property();
    bool is_custom_property = !property_name.empty() && property_name.starts_with("--");
    if (is_custom_property) {
        property = Property::Custom;
    }
    skip_whitespace_tokens();
    if (!match(TokenType::Colon)) {
        while (!eof() && peek().type != TokenType::Semicolon && peek().type != TokenType::RBrace) {
            advance();
        }
        match(TokenType::Semicolon);
        return false;
    }
    skip_whitespace_tokens();
    auto push_decl = [&](Property decl_property, Value value) {
        Declaration decl;
        decl.property = decl_property;
        decl.value = std::move(value);
        decls.push_back(std::move(decl));
    };
    if (property == Property::Custom) {
        std::string raw_value = parse_custom_property_value();
        match(TokenType::Semicolon);  // consume if present
        if (!raw_value.empty()) {
            Declaration decl;
            decl.property = Property::Custom;
            decl.custom_property = std::string(property_name);
            decl.value = Value::identifier(std::move(raw_value));
            decls.push_back(std::move(decl));
        }
        return true;
    }
    if (property == Property::FontFamily) {
        std::string list = parse_font_family_list();
        match(TokenType::Semicolon);  // consume if present
        if (!list.empty()) {
            push_decl(property, Value::identifier(std::move(list)));
        }
        return true;
    }
    std::vector<Value> values = parse_value_list();
    match(TokenType::Semicolon);  // consume if present
    if (values.empty()) {
        return false;
    }

    auto value_to_text = [](const Value& value) -> std::string {
        if (value.type == Value::Type::Identifier) {
            return value.ident;
        }
        if (value.type == Value::Type::Color) {
            return Core::Utils::color_to_hex(value.color);
        }
        if (value.type == Value::Type::Length) {
            std::string out = std::to_string(value.length.value);
            if (value.length.unit == Unit::Px) {
                out += "px";
            } else if (value.length.unit == Unit::Em) {
                out += "em";
            }
            return out;
        }
        if (value.type == Value::Type::Number) {
            return std::to_string(value.number);
        }
        return "";
    };

    auto join_value_list = [&](const std::vector<Value>& list) -> std::string {
        std::string out;
        for (const auto& value : list) {
            std::string piece = value_to_text(value);
            if (piece.empty()) {
                continue;
            }
            if (!out.empty()) {
                out.push_back(' ');
            }
            out += piece;
        }
        return out;
    };

    auto build_var_expression = [&](const std::vector<Value>& list) -> std::string {
        if (list.empty()) {
            return "";
        }
        if (list[0].type != Value::Type::Identifier || list[0].ident != "var") {
            return "";
        }
        if (list.size() < 2 || list[1].type != Value::Type::Identifier) {
            return "";
        }
        std::string expr = "var(";
        expr += list[1].ident;
        if (list.size() >= 3) {
            std::string fallback = value_to_text(list[2]);
            if (!fallback.empty()) {
                expr += ", ";
                expr += fallback;
            }
        }
        expr += ")";
        return expr;
    };

    if (property == Property::Color || property == Property::BackgroundColor || property == Property::Background) {
        std::string var_expr = build_var_expression(values);
        if (!var_expr.empty()) {
            values.clear();
            values.push_back(Value::identifier(std::move(var_expr)));
        }
    }

    if (property == Property::Unknown && !property_name.empty()) {
        if (m_unknown_properties.should_log(property_name)) {
            HB_LOG_WARN("[parser] Unsupported CSS property encountered: " << property_name);
        }
        return true;
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
        push_decl(top, top_value);
        push_decl(right, right_value);
        push_decl(bottom, bottom_value);
        push_decl(left, left_value);
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
            if (!border_style && value.type == Value::Type::Identifier &&
                (value.ident == ValueNames::Solid || value.ident == ValueNames::Outset ||
                 value.ident == ValueNames::Inset || value.ident == ValueNames::Ridge ||
                 value.ident == ValueNames::Groove)) {
                border_style = value;
            }
        }
        if (border_width) push_decl(Property::BorderWidth, *border_width);
        if (border_style) push_decl(Property::BorderStyle, *border_style);
        if (border_color) push_decl(Property::BorderColor, *border_color);
        return true;
    }
    if (property == Property::BackgroundImage) {
        for (const auto& value : values) {
            if (value.type == Value::Type::Url) {
                push_decl(Property::BackgroundImage, value);
                return true;
            }
            if (value.type == Value::Type::Identifier && value.ident == ValueNames::None) {
                push_decl(Property::BackgroundImage, value);
                return true;
            }
        }
        return true;
    }
    if (property == Property::BackgroundRepeat || property == Property::BackgroundPosition ||
        property == Property::BackgroundSize) {
        std::string text = join_value_list(values);
        if (!text.empty()) {
            push_decl(property, Value::identifier(std::move(text)));
        }
        return true;
    }
    if (property == Property::ListStyle) {
        std::string text = join_value_list(values);
        if (!text.empty()) {
            push_decl(property, Value::identifier(std::move(text)));
        }
        return true;
    }
    if (property == Property::Transform) {
        std::string text = join_value_list(values);
        if (!text.empty()) {
            push_decl(property, Value::identifier(std::move(text)));
        }
        return true;
    }
    if (property == Property::Background) {
        std::vector<Value> position_values;
        for (const auto& value : values) {
            if (value.type == Value::Type::Color) {
                push_decl(Property::BackgroundColor, value);
                continue;
            }
            if (value.type == Value::Type::Identifier && value.ident.starts_with("var(")) {
                push_decl(Property::BackgroundColor, value);
                continue;
            }
            if (value.type == Value::Type::Url) {
                push_decl(Property::BackgroundImage, value);
                continue;
            }
            if (value.type == Value::Type::Identifier) {
                if (value.ident == ValueNames::Repeat || value.ident == ValueNames::NoRepeat ||
                    value.ident == ValueNames::RepeatX || value.ident == ValueNames::RepeatY) {
                    push_decl(Property::BackgroundRepeat, value);
                    continue;
                }
                if (value.ident == ValueNames::Cover || value.ident == ValueNames::Contain ||
                    value.ident == ValueNames::Auto) {
                    push_decl(Property::BackgroundSize, value);
                    continue;
                }
                if (value.ident == ValueNames::Left || value.ident == ValueNames::Right ||
                    value.ident == ValueNames::Center || value.ident == ValueNames::Top ||
                    value.ident == ValueNames::Bottom) {
                    position_values.push_back(value);
                    continue;
                }
            }
            if (value.type == Value::Type::Length || value.type == Value::Type::Number) {
                position_values.push_back(value);
            }
        }
        if (!position_values.empty()) {
            std::string position_text = join_value_list(position_values);
            if (!position_text.empty()) {
                push_decl(Property::BackgroundPosition, Value::identifier(std::move(position_text)));
            }
        }
        return true;
    }

    Value value = values.empty() ? Value::identifier("") : values.front();
    push_decl(property, value);
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
