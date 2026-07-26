#include "style/parser/CssParser.h"

#include <cstdlib>
#include <optional>
#include <ostream>
#include <utility>

#include "core/GraphicsTypes.h"
#include "core/utils/ColorUtils.h"
#include "core/utils/Log.h"
#include "core/utils/ParseUtils.h"
#include "core/utils/StringUtils.h"
#include "style/compute/StyleValueUtils.h"
#include "style/parser/CssValueUtils.h"
#include "style/registry/CssPropertyRegistry.h"
#include "style/registry/CssValueNames.h"

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
    if (!is_selector_start(peek().type)) {
        return selector;
    }

    selector.parts.push_back(parse_simple_selector());
    while (true) {
        bool saw_whitespace = false;
        while (peek().type == TokenType::Whitespace) {
            saw_whitespace = true;
            advance();
        }

        std::optional<Selector::Combinator> explicit_combinator;
        if (match(TokenType::Greater)) {
            explicit_combinator = Selector::Combinator::Child;
        } else if (match(TokenType::Plus)) {
            explicit_combinator = Selector::Combinator::NextSibling;
        } else if (match(TokenType::Tilde)) {
            explicit_combinator = Selector::Combinator::SubsequentSibling;
        }
        if (explicit_combinator) {
            while (peek().type == TokenType::Whitespace) {
                advance();
            }
            if (!is_selector_start(peek().type)) {
                break;
            }
            selector.combinators.push_back(*explicit_combinator);
            selector.parts.push_back(parse_simple_selector());
            continue;
        }

        if (!saw_whitespace || !is_selector_start(peek().type)) {
            break;
        }
        selector.combinators.push_back(Selector::Combinator::Descendant);
        selector.parts.push_back(parse_simple_selector());
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
        if (match(TokenType::Colon)) {
            if (peek().type == TokenType::Identifier) {
                auto pseudo = Core::Utils::to_lower(std::string(advance().lexeme));
                if (pseudo == "hover") {
                    selector.pseudo_classes.push_back(SelectorPart::PseudoClass::Hover);
                } else if (pseudo == "active") {
                    selector.pseudo_classes.push_back(SelectorPart::PseudoClass::Active);
                } else if (pseudo == "focus") {
                    selector.pseudo_classes.push_back(SelectorPart::PseudoClass::Focus);
                } else if (pseudo == "visited") {
                    selector.pseudo_classes.push_back(SelectorPart::PseudoClass::Visited);
                }
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

// Consumes `!important` (whitespace allowed after the bang). Called with peek()
// on a Bang token; a bang followed by anything else is consumed and ignored.
bool Parser::consume_important_flag() {
    advance();  // '!'
    skip_whitespace_tokens();
    if (peek().type == TokenType::Identifier && Core::Utils::to_lower(std::string(peek().lexeme)) == "important") {
        advance();
        return true;
    }
    return false;
}

std::vector<Value> Parser::parse_value_list(bool* important) {
    std::vector<Value> values;
    while (!eof() && peek().type != TokenType::Semicolon && peek().type != TokenType::RBrace) {
        if (peek().type == TokenType::Whitespace) {
            advance();
            continue;
        }
        if (peek().type == TokenType::Bang) {
            bool flagged = consume_important_flag();
            if (important) *important |= flagged;
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
        // Preserve `/` as a marker so shorthands can split on it (the
        // `background` position/size and `font` size/line-height separator).
        if (peek().type == TokenType::Slash) {
            values.push_back(Value::identifier("/"));
            advance();
            continue;
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
    if (ident == "rgb" || ident == "rgba") {
        if (auto color = parse_color_function()) {
            return Value::color_value(*color);
        }
        return Value::identifier(std::move(ident));
    }
    if (ident == "calc") {
        if (auto calc = parse_calc()) {
            return Value::calc_value(*calc);
        }
        // Unsupported calc() form: emit a placeholder that no applier accepts, so
        // the declaration is dropped rather than misapplied.
        return Value::identifier("calc");
    }
    if (auto color = parse_named_color(ident)) {
        return Value::color_value(*color);
    }
    return Value::identifier(std::move(ident));
}

// Parses the argument list of rgb()/rgba(). The tokenizer drops parentheses,
// so "rgba(0, 0, 0, .15)" arrives as [0][,][0][,][0][,][.15]. Channels may be
// fractional (DDG uses rgba(137.5,...)); alpha is 0..1 and scales to 0..255.
std::optional<Color> Parser::parse_color_function() {
    auto read_number = [&]() -> std::optional<float> {
        skip_whitespace_tokens();
        if (peek().type != TokenType::Number) {
            return std::nullopt;
        }
        std::string text(advance().lexeme);
        float value = Core::Utils::parse_float(text).value_or(0.0f);
        if (match(TokenType::Percent)) {
            value = value * 255.0f / 100.0f;
        }
        return value;
    };
    auto expect_comma = [&]() {
        skip_whitespace_tokens();
        return match(TokenType::Comma);
    };

    auto r = read_number();
    if (!r || !expect_comma()) return std::nullopt;
    auto g = read_number();
    if (!g || !expect_comma()) return std::nullopt;
    auto b = read_number();
    if (!b) return std::nullopt;

    float alpha = 1.0f;
    skip_whitespace_tokens();
    if (match(TokenType::Comma)) {
        skip_whitespace_tokens();
        if (peek().type != TokenType::Number) {
            return std::nullopt;
        }
        std::string text(advance().lexeme);
        alpha = Core::Utils::parse_float(text).value_or(1.0f);
        if (match(TokenType::Percent)) {
            alpha /= 100.0f;
        }
    }

    auto clamp_channel = [](float value) {
        if (value < 0.0f) value = 0.0f;
        if (value > 255.0f) value = 255.0f;
        return static_cast<uint8_t>(value + 0.5f);
    };
    Color color;
    color.r = clamp_channel(*r);
    color.g = clamp_channel(*g);
    color.b = clamp_channel(*b);
    if (alpha < 0.0f) alpha = 0.0f;
    if (alpha > 1.0f) alpha = 1.0f;
    color.a = static_cast<uint8_t>(alpha * 255.0f + 0.5f);
    return color;
}

// Parses the additive subset of calc(): px and % terms joined by +/- (CSS
// requires whitespace around the operators, so `-` arrives as its own token).
// The tokenizer drops parentheses, so the expression simply runs to the end of
// the value. Anything outside the subset (multiplication, nested calc, other
// units, var()) fails and returns nullopt so the declaration is dropped. Called
// with the leading `calc` identifier already consumed.
std::optional<Value::Calc> Parser::parse_calc() {
    Value::Calc sum;
    float sign = 1.0f;
    bool expect_term = true;
    bool ok = true;
    bool any_term = false;
    while (!eof()) {
        TokenType type = peek().type;
        if (type == TokenType::Semicolon || type == TokenType::RBrace || type == TokenType::Bang ||
            type == TokenType::End) {
            break;
        }
        if (type == TokenType::Whitespace) {
            advance();
            continue;
        }
        if (expect_term) {
            if (type != TokenType::Number) {
                ok = false;
                advance();
                continue;
            }
            float number = Core::Utils::parse_float(std::string(advance().lexeme)).value_or(0.0f);
            if (match(TokenType::Percent)) {
                sum.percent += sign * number;
                sum.has_percent = true;
            } else if (peek().type == TokenType::Identifier) {
                auto unit = StyleValueUtils::parse_unit_token(peek().lexeme);
                if (unit == Unit::Px) {
                    advance();
                    sum.px += sign * number;
                } else {
                    ok = false;  // em/other units unsupported in calc for now
                    advance();
                }
            } else {
                ok = false;  // a bare number is only valid as a multiplier
            }
            any_term = true;
            expect_term = false;
            continue;
        }
        if (type == TokenType::Plus) {
            advance();
            sign = 1.0f;
            expect_term = true;
            continue;
        }
        if (type == TokenType::Identifier && peek().lexeme == "-") {
            advance();
            sign = -1.0f;
            expect_term = true;
            continue;
        }
        // Any other operator (*, /) or stray token is outside the subset.
        ok = false;
        advance();
    }
    if (!ok || !any_term || expect_term) {
        return std::nullopt;
    }
    return sum;
}

Value Parser::parse_number_value() {
    std::string number_text = std::string(advance().lexeme);
    float number = Core::Utils::parse_float(number_text).value_or(0.0f);
    if (match(TokenType::Percent)) {
        return Value::length_value(number, Unit::Percent);
    }
    if (peek().type == TokenType::Identifier) {
        std::string unit_text = std::string(advance().lexeme);
        Unit unit = Unit::Unknown;
        if (auto parsed = StyleValueUtils::parse_unit_token(unit_text)) {
            unit = *parsed;
        }
        return Value::length_value(number, unit);
    }
    return Value::number_value(number);
}

std::string Parser::parse_font_family_list(bool* important) {
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
        if (peek().type == TokenType::Bang) {
            bool flagged = consume_important_flag();
            if (important) *important |= flagged;
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

// Reads one grid track (a `<track-size>`) from the token stream and returns its
// canonical text: "Npx" | "Nem" | "N%" | "Nfr" | "auto". The tokenizer splits
// "100px" into Number + Identifier and "50%" into Number + Percent, so we peek
// the unit that follows a number. Unknown/unsupported sizes collapse to "auto".
std::string Parser::read_one_grid_track() {
    skip_whitespace_tokens();
    if (peek().type == TokenType::Identifier) {
        std::string id = Core::Utils::to_lower(std::string(advance().lexeme));
        if (id == ValueNames::Auto || id == "min-content" || id == "max-content") {
            return "auto";
        }
        return {};  // unsupported keyword (minmax/fit-content/etc.) -> skip
    }
    if (peek().type == TokenType::Number) {
        std::string num(advance().lexeme);
        if (peek().type == TokenType::Percent) {
            advance();
            return num + "%";
        }
        if (peek().type == TokenType::Identifier) {
            std::string unit = Core::Utils::to_lower(std::string(peek().lexeme));
            if (unit == "fr" || unit == ValueNames::Px || unit == ValueNames::Em) {
                advance();
                return num + unit;
            }
            advance();          // unknown unit
            return num + "px";  // lenient: treat as px
        }
        return num + "px";  // unitless -> px
    }
    advance();  // slash/comma/other -> skip
    return {};
}

std::string Parser::parse_grid_track_list_text(bool* important) {
    std::vector<std::string> tracks;
    while (!eof() && peek().type != TokenType::Semicolon && peek().type != TokenType::RBrace) {
        if (peek().type == TokenType::Whitespace || peek().type == TokenType::Comma) {
            advance();
            continue;
        }
        if (peek().type == TokenType::Bang) {
            bool flagged = consume_important_flag();
            if (important) *important |= flagged;
            continue;
        }
        if (peek().type == TokenType::Identifier &&
            Core::Utils::to_lower(std::string(peek().lexeme)) == ValueNames::Repeat) {
            // repeat(<count>, <track-list>). The tokenizer drops the parentheses,
            // so we cannot see where the group ends: MVP treats the repeat as
            // consuming the remainder of the track list (repeat() must be the last
            // component). Common forms — `repeat(3, 1fr)`, `100px repeat(2, 1fr)` —
            // work; `repeat(...) 100px` (trailing tracks) does not.
            // Fixed centrally by T-CSS-PAREN-TOKENS-1 (tokenize parens).
            advance();  // "repeat"
            skip_whitespace_tokens();
            int count = 0;
            if (peek().type == TokenType::Number) {
                count = static_cast<int>(Core::Utils::parse_float(advance().lexeme).value_or(0.0f));
            }
            skip_whitespace_tokens();
            if (peek().type == TokenType::Comma) advance();
            std::vector<std::string> repeated;
            while (!eof() && peek().type != TokenType::Semicolon && peek().type != TokenType::RBrace) {
                if (peek().type == TokenType::Whitespace || peek().type == TokenType::Comma) {
                    advance();
                    continue;
                }
                if (peek().type == TokenType::Bang) {
                    bool flagged = consume_important_flag();
                    if (important) *important |= flagged;
                    continue;
                }
                std::string track = read_one_grid_track();
                if (!track.empty()) repeated.push_back(std::move(track));
            }
            if (count > 0 && count <= 1000) {
                for (int i = 0; i < count; ++i) {
                    tracks.insert(tracks.end(), repeated.begin(), repeated.end());
                }
            }
            break;  // repeat consumed the remainder
        }
        std::string track = read_one_grid_track();
        if (!track.empty()) tracks.push_back(std::move(track));
    }
    std::string out;
    for (const auto& track : tracks) {
        if (!out.empty()) out.push_back(' ');
        out += track;
    }
    return out;
}

std::string Parser::parse_grid_placement_text(bool* important) {
    // Reads `<start> [ / <end> ]`, where each side is a line number, `span N`, or
    // `auto`. Canonicalized to "line/span" (line 0 = auto-place, span >= 1).
    struct Side {
        bool is_span = false;
        bool is_auto = false;
        int value = 0;  // line number, or span count when is_span
    };
    auto read_side = [&]() -> Side {
        Side side;
        while (!eof() && peek().type != TokenType::Semicolon && peek().type != TokenType::RBrace &&
               peek().type != TokenType::Slash) {
            if (peek().type == TokenType::Whitespace) {
                advance();
                continue;
            }
            if (peek().type == TokenType::Bang) {
                bool flagged = consume_important_flag();
                if (important) *important |= flagged;
                continue;
            }
            if (peek().type == TokenType::Identifier) {
                std::string id = Core::Utils::to_lower(std::string(advance().lexeme));
                if (id == ValueNames::Span) {
                    side.is_span = true;
                } else if (id == ValueNames::Auto) {
                    side.is_auto = true;
                }
                continue;
            }
            if (peek().type == TokenType::Number) {
                side.value = static_cast<int>(Core::Utils::parse_float(advance().lexeme).value_or(0.0f));
                continue;
            }
            advance();
        }
        return side;
    };

    Side start = read_side();
    Side end;
    bool has_end = false;
    if (peek().type == TokenType::Slash) {
        advance();
        end = read_side();
        has_end = true;
    }

    int line = 0;  // 0 = auto
    int span = 1;
    if (start.is_span) {
        span = std::max(1, start.value);
        if (has_end && !end.is_span && !end.is_auto && end.value != 0) {
            line = end.value - span;  // `span S / M`
        }
    } else if (!start.is_auto && start.value != 0) {
        line = start.value;
        if (has_end) {
            if (end.is_span) {
                span = std::max(1, end.value);
            } else if (!end.is_auto && end.value != 0) {
                span = std::max(1, end.value - start.value);  // `N / M`
            }
        }
    } else if (has_end && end.is_span) {
        span = std::max(1, end.value);  // `auto / span S`
    }
    if (line < 0) line = 0;
    return std::to_string(line) + "/" + std::to_string(span);
}

std::string Parser::parse_gap_text(bool* important) {
    // gap: <row-gap> [<column-gap>]. Each is a length; canonicalized to
    // "<row> <col>" (one value applies to both). % is unsupported (dropped).
    std::vector<std::string> parts;
    while (!eof() && peek().type != TokenType::Semicolon && peek().type != TokenType::RBrace && parts.size() < 2) {
        if (peek().type == TokenType::Whitespace) {
            advance();
            continue;
        }
        if (peek().type == TokenType::Bang) {
            bool flagged = consume_important_flag();
            if (important) *important |= flagged;
            continue;
        }
        std::string track = read_one_grid_track();  // reuses the length reader
        if (!track.empty()) parts.push_back(std::move(track));
    }
    if (parts.empty()) return {};
    if (parts.size() == 1) return parts[0] + " " + parts[0];
    return parts[0] + " " + parts[1];
}

std::string Parser::parse_custom_property_value(bool* important) {
    std::string raw;
    while (!eof() && peek().type != TokenType::Semicolon && peek().type != TokenType::RBrace) {
        if (peek().type == TokenType::Bang) {
            bool flagged = consume_important_flag();
            if (important) *important |= flagged;
            continue;
        }
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
    bool important = false;
    auto push_decl = [&](Property decl_property, Value value) {
        Declaration decl;
        decl.property = decl_property;
        decl.value = std::move(value);
        decl.important = important;
        decls.push_back(std::move(decl));
    };
    if (property == Property::Custom) {
        std::string raw_value = parse_custom_property_value(&important);
        match(TokenType::Semicolon);  // consume if present
        if (!raw_value.empty()) {
            Declaration decl;
            decl.property = Property::Custom;
            decl.custom_property = std::string(property_name);
            decl.value = Value::identifier(std::move(raw_value));
            decl.important = important;
            decls.push_back(std::move(decl));
        }
        return true;
    }

    PropertyRegistry::ParserHook parser_hook = PropertyRegistry::parser_hook(property);
    if (parser_hook == PropertyRegistry::ParserHook::parse_font_family) {
        // font-family has a dedicated list parser, but a whole-value var()
        // expression must stay packaged so StyleEngine can substitute the custom
        // property after cascade/inheritance. Without this branch,
        // `var(--font-family)` was flattened into the literal family name
        // "var --font-family" and warned on every text measurement.
        skip_whitespace_tokens();
        if (peek().type == TokenType::Identifier && peek().lexeme == "var") {
            std::vector<Value> values = parse_value_list(&important);
            std::string var_expr = build_var_expression(values);
            match(TokenType::Semicolon);  // consume if present
            if (!var_expr.empty()) {
                push_decl(property, Value::identifier(std::move(var_expr)));
            }
            return true;
        }
        std::string list = parse_font_family_list(&important);
        match(TokenType::Semicolon);  // consume if present
        if (!list.empty()) {
            push_decl(property, Value::identifier(std::move(list)));
        }
        return true;
    }
    // Grid values read raw tokens (repeat()/fr/spans need control the generic
    // value list can't give) and canonicalize to a string the apply hook decodes.
    if (parser_hook == PropertyRegistry::ParserHook::parse_grid_track_list ||
        parser_hook == PropertyRegistry::ParserHook::parse_grid_placement ||
        parser_hook == PropertyRegistry::ParserHook::parse_gap) {
        std::string canonical;
        if (parser_hook == PropertyRegistry::ParserHook::parse_grid_track_list) {
            canonical = parse_grid_track_list_text(&important);
        } else if (parser_hook == PropertyRegistry::ParserHook::parse_grid_placement) {
            canonical = parse_grid_placement_text(&important);
        } else {
            canonical = parse_gap_text(&important);
        }
        match(TokenType::Semicolon);  // consume if present
        if (!canonical.empty()) {
            push_decl(property, Value::identifier(std::move(canonical)));
        }
        return true;
    }
    std::vector<Value> values = parse_value_list(&important);
    match(TokenType::Semicolon);  // consume if present
    if (values.empty()) {
        return false;
    }

    // Package var() for any property (T-CSS-VAR-3): a whole-value
    // `var(--x[, fallback])` keeps its fallback; var() terms inside longer
    // lists (border-radius corners) merge pairwise without fallback support.
    // The StyleEngine substitutes the custom property's value at apply time.
    std::string var_expr = build_var_expression(values);
    if (!var_expr.empty()) {
        values.clear();
        values.push_back(Value::identifier(std::move(var_expr)));
    } else {
        merge_var_terms(values);
    }

    if (property == Property::Unknown && !property_name.empty()) {
        // Unrecognized vendor-prefixed properties are non-standard; drop them
        // silently (T-CSS-COMPAT-ALIAS-1) so legacy CSS does not flood the log.
        // Unknown standard properties still warn, keeping real gaps visible.
        if (!PropertyRegistry::is_vendor_prefixed_name(property_name) &&
            m_unknown_properties.should_log(property_name)) {
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

    switch (parser_hook) {
        case PropertyRegistry::ParserHook::parse_margin_shorthand:
            emit_edges(Property::MarginTop, Property::MarginRight, Property::MarginBottom, Property::MarginLeft);
            return true;
        case PropertyRegistry::ParserHook::parse_padding_shorthand:
            emit_edges(Property::PaddingTop, Property::PaddingRight, Property::PaddingBottom, Property::PaddingLeft);
            return true;
        case PropertyRegistry::ParserHook::parse_border_shorthand: {
            auto border_width_property_for = [&](Property target) {
                switch (target) {
                    case Property::BorderTop:
                        return Property::BorderTopWidth;
                    case Property::BorderRight:
                        return Property::BorderRightWidth;
                    case Property::BorderBottom:
                        return Property::BorderBottomWidth;
                    case Property::BorderLeft:
                        return Property::BorderLeftWidth;
                    default:
                        return Property::BorderWidth;
                }
            };
            // `border: none` (or `0 none`) removes the border entirely.
            for (const auto& value : values) {
                if (value.type == Value::Type::Identifier && value.ident == ValueNames::None) {
                    push_decl(border_width_property_for(property), Value::length_value(0.0f, Unit::Px));
                    push_decl(Property::BorderStyle, Value::identifier(std::string(ValueNames::None)));
                    return true;
                }
            }
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
            if (border_width) {
                push_decl(border_width_property_for(property), *border_width);
            }
            if (border_style) push_decl(Property::BorderStyle, *border_style);
            if (border_color) push_decl(Property::BorderColor, *border_color);
            return true;
        }
        case PropertyRegistry::ParserHook::parse_border_radius: {
            // `border-radius` shorthand: 1-4 lengths mapped to the four corners.
            // (The elliptical `/ <vertical>` syntax is not supported; DDG's
            // controls use circular radii only.) Vendor-prefixed shorthands
            // resolve to this same property, so they land here too.
            std::vector<Value> radii;
            for (const auto& value : values) {
                if (value.type == Value::Type::Length || value.type == Value::Type::Number) {
                    radii.push_back(value);
                } else if (value.type == Value::Type::Identifier && value.ident.starts_with("var(")) {
                    // Resolved per-corner at apply time (DDG: `border-radius:
                    // 0 var(--default-border-radius) var(--default-border-radius) 0`).
                    radii.push_back(value);
                }
                if (radii.size() == 4) {
                    break;
                }
            }
            if (radii.empty()) {
                return true;
            }
            const Value& tl = radii[0];
            const Value& tr = radii.size() > 1 ? radii[1] : radii[0];
            const Value& br = radii.size() > 2 ? radii[2] : radii[0];
            const Value& bl = radii.size() > 3 ? radii[3] : (radii.size() > 1 ? radii[1] : radii[0]);
            push_decl(Property::BorderTopLeftRadius, tl);
            push_decl(Property::BorderTopRightRadius, tr);
            push_decl(Property::BorderBottomRightRadius, br);
            push_decl(Property::BorderBottomLeftRadius, bl);
            return true;
        }
        case PropertyRegistry::ParserHook::parse_font_shorthand: {
            std::optional<Value> font_style;
            std::optional<Value> font_weight;
            std::optional<Value> font_size;
            std::optional<Value> line_height;
            std::vector<Value> font_family_tokens;

            bool after_size = false;
            for (const auto& value : values) {
                // The `/` between font-size and line-height is a separator, not
                // a family token.
                if (value.type == Value::Type::Identifier && value.ident == "/") {
                    continue;
                }
                if (!after_size) {
                    if (value.type == Value::Type::Identifier &&
                        (value.ident == ValueNames::Italic || value.ident == ValueNames::Normal)) {
                        font_style = value;
                        continue;
                    }
                    if ((value.type == Value::Type::Identifier &&
                         (value.ident == ValueNames::Bold || value.ident == ValueNames::Normal)) ||
                        value.type == Value::Type::Number) {
                        font_weight = value;
                        continue;
                    }
                    if (value.type == Value::Type::Length) {
                        font_size = value;
                        after_size = true;
                        continue;
                    }
                } else if (!line_height && (value.type == Value::Type::Length || value.type == Value::Type::Number)) {
                    line_height = value;
                    continue;
                }

                if (after_size) {
                    font_family_tokens.push_back(value);
                }
            }

            if (font_style) {
                push_decl(Property::FontStyle, *font_style);
            }
            if (font_weight) {
                push_decl(Property::FontWeight, *font_weight);
            }
            if (font_size) {
                push_decl(Property::FontSize, *font_size);
            }
            if (line_height) {
                push_decl(Property::LineHeight, *line_height);
            }
            if (!font_family_tokens.empty()) {
                std::string family = join_value_list(font_family_tokens);
                if (!family.empty()) {
                    push_decl(Property::FontFamily, Value::identifier(std::move(family)));
                }
            }
            return true;
        }
        case PropertyRegistry::ParserHook::parse_background_image:
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
        case PropertyRegistry::ParserHook::parse_background_repeat:
        case PropertyRegistry::ParserHook::parse_background_position:
        case PropertyRegistry::ParserHook::parse_background_size:
        case PropertyRegistry::ParserHook::parse_list_style_shorthand:
        case PropertyRegistry::ParserHook::parse_transform: {
            std::string text = join_value_list(values);
            if (!text.empty()) {
                push_decl(property, Value::identifier(std::move(text)));
            }
            return true;
        }
        case PropertyRegistry::ParserHook::parse_passthrough: {
            // Recognized-but-inert properties (e.g. transition, transform-origin):
            // consume the whole value so it is not reported as unsupported. The
            // applier ignores it (static application, T-ANIM-1).
            push_decl(property, Value::identifier(join_value_list(values)));
            return true;
        }
        case PropertyRegistry::ParserHook::parse_outline_shorthand: {
            std::optional<Value> outline_width;
            std::optional<Value> outline_color;
            for (const auto& value : values) {
                if (!outline_width && value.type == Value::Type::Length) {
                    outline_width = value;
                    continue;
                }
                if (!outline_color && value.type == Value::Type::Color) {
                    outline_color = value;
                    continue;
                }
            }
            if (outline_width) {
                push_decl(Property::OutlineWidth, *outline_width);
            }
            if (outline_color) {
                push_decl(Property::OutlineColor, *outline_color);
            }
            if (!outline_width && !outline_color) {
                std::string text = join_value_list(values);
                if (!text.empty()) {
                    push_decl(Property::Outline, Value::identifier(std::move(text)));
                }
            }
            return true;
        }
        case PropertyRegistry::ParserHook::parse_clip: {
            // Legacy `clip: rect(top, right, bottom, left)`. The tokenizer drops
            // the parens and commas, so a rect arrives as [ident "rect"] followed
            // by four edge tokens. `clip: auto` (or any non-rect) leaves it
            // unclipped; emit the identifier so apply resets a prior clip.
            if (values.empty()) {
                return true;
            }
            if (values[0].type != Value::Type::Identifier || values[0].ident != "rect") {
                push_decl(property, values[0]);
                return true;
            }
            Value::Clip clip;
            std::optional<Length>* edges[4] = {&clip.top, &clip.right, &clip.bottom, &clip.left};
            size_t edge_index = 0;
            for (size_t i = 1; i < values.size() && edge_index < 4; ++i) {
                const Value& edge = values[i];
                if (edge.type == Value::Type::Identifier && edge.ident == ValueNames::Auto) {
                    *edges[edge_index++] = std::nullopt;
                } else if (edge.type == Value::Type::Length) {
                    *edges[edge_index++] = edge.length;
                } else if (edge.type == Value::Type::Number) {
                    *edges[edge_index++] = Length{edge.number, Unit::Px};
                }
            }
            if (edge_index == 4) {
                push_decl(property, Value::clip_value(clip));
            }
            return true;
        }
        case PropertyRegistry::ParserHook::parse_box_shadow: {
            if (values.size() == 1 && values[0].type == Value::Type::Identifier &&
                values[0].ident == ValueNames::None) {
                push_decl(property, values[0]);
                return true;
            }

            std::vector<Length> lengths;
            std::optional<Color> color;
            for (const auto& value : values) {
                if (value.type == Value::Type::Color && !color) {
                    color = value.color;
                    continue;
                }
                if (value.type == Value::Type::Length) {
                    lengths.push_back(value.length);
                    continue;
                }
                if (value.type == Value::Type::Number) {
                    Length length;
                    length.value = value.number;
                    length.unit = Unit::Px;
                    lengths.push_back(length);
                }
            }

            if (lengths.size() < 2) {
                return true;
            }

            Value::Shadow shadow;
            shadow.offset_x = lengths[0];
            shadow.offset_y = lengths[1];
            if (lengths.size() >= 3) {
                shadow.blur = lengths[2];
            }
            if (color) {
                shadow.color = *color;
            }

            push_decl(property, Value::shadow_value(shadow));
            return true;
        }
        case PropertyRegistry::ParserHook::parse_background_shorthand: {
            // A whole-value var() was already packaged into a single
            // identifier above; treat it as the color layer (legacy behavior).
            if (values.size() == 1 && values[0].type == Value::Type::Identifier &&
                values[0].ident.starts_with("var(")) {
                push_decl(Property::BackgroundColor, values[0]);
                return true;
            }

            std::vector<Value> position_values;
            std::vector<Value> size_values;
            // In `<position> / <size>`, everything after the slash is the size.
            bool after_slash = false;
            for (const auto& value : values) {
                if (value.type == Value::Type::Identifier && value.ident == "/") {
                    after_slash = true;
                    continue;
                }
                if (value.type == Value::Type::Color) {
                    push_decl(Property::BackgroundColor, value);
                    continue;
                }
                if (value.type == Value::Type::Url) {
                    push_decl(Property::BackgroundImage, value);
                    continue;
                }
                if (value.type == Value::Type::Identifier) {
                    if (value.ident == ValueNames::None) {
                        // `background: none` clears both color and image.
                        push_decl(Property::BackgroundColor, value);
                        push_decl(Property::BackgroundImage, Value::identifier(std::string(ValueNames::None)));
                        continue;
                    }
                    if (value.ident == ValueNames::Transparent) {
                        // `transparent` is a color layer only; it must never clear an
                        // image from the same shorthand (e.g. "url(x), linear-gradient
                        // (transparent, transparent)" keeps the url layer).
                        push_decl(Property::BackgroundColor, value);
                        continue;
                    }
                    if (value.ident == ValueNames::Repeat || value.ident == ValueNames::NoRepeat ||
                        value.ident == ValueNames::RepeatX || value.ident == ValueNames::RepeatY) {
                        push_decl(Property::BackgroundRepeat, value);
                        continue;
                    }
                    if (value.ident == ValueNames::Cover || value.ident == ValueNames::Contain) {
                        push_decl(Property::BackgroundSize, value);
                        continue;
                    }
                    if (value.ident == ValueNames::Auto) {
                        (after_slash ? size_values : position_values).push_back(value);
                        continue;
                    }
                    if (value.ident == ValueNames::Left || value.ident == ValueNames::Right ||
                        value.ident == ValueNames::Center || value.ident == ValueNames::Top ||
                        value.ident == ValueNames::Bottom) {
                        (after_slash ? size_values : position_values).push_back(value);
                        continue;
                    }
                }
                if (value.type == Value::Type::Length || value.type == Value::Type::Number) {
                    (after_slash ? size_values : position_values).push_back(value);
                }
            }
            if (!position_values.empty()) {
                std::string position_text = join_value_list(position_values);
                if (!position_text.empty()) {
                    push_decl(Property::BackgroundPosition, Value::identifier(std::move(position_text)));
                }
            }
            if (!size_values.empty()) {
                std::string size_text = join_value_list(size_values);
                if (!size_text.empty()) {
                    push_decl(Property::BackgroundSize, Value::identifier(std::move(size_text)));
                }
            }
            return true;
        }
        case PropertyRegistry::ParserHook::parse_flex_shorthand: {
            // flex: none | auto | <grow> <shrink>? <basis>?
            std::optional<Value> grow;
            std::optional<Value> shrink;
            std::optional<Value> basis;
            if (values.size() == 1 && values[0].type == Value::Type::Identifier) {
                if (values[0].ident == ValueNames::None) {
                    grow = Value::number_value(0.0f);
                    shrink = Value::number_value(0.0f);
                    basis = Value::identifier(std::string(ValueNames::Auto));
                } else if (values[0].ident == ValueNames::Auto) {
                    grow = Value::number_value(1.0f);
                    shrink = Value::number_value(1.0f);
                    basis = Value::identifier(std::string(ValueNames::Auto));
                }
            }
            if (!grow && !shrink && !basis) {
                for (const auto& value : values) {
                    if (value.type == Value::Type::Number) {
                        if (!grow) {
                            grow = value;
                            continue;
                        }
                        if (!shrink) {
                            shrink = value;
                            continue;
                        }
                    } else if (!basis && (value.type == Value::Type::Length ||
                                          (value.type == Value::Type::Identifier && value.ident == ValueNames::Auto))) {
                        basis = value;
                    }
                }
                // Unitless single-value form (`flex: 1`) implies basis 0.
                if (grow && !basis) {
                    basis = Value::length_value(0.0f, Unit::Px);
                }
            }
            if (grow) push_decl(Property::FlexGrow, *grow);
            if (shrink) push_decl(Property::FlexShrink, *shrink);
            if (basis) push_decl(Property::FlexBasis, *basis);
            return true;
        }
        case PropertyRegistry::ParserHook::Unknown:
        case PropertyRegistry::ParserHook::parse_identifier:
        case PropertyRegistry::ParserHook::parse_font_size:
        case PropertyRegistry::ParserHook::parse_length_number:
        case PropertyRegistry::ParserHook::parse_length_auto:
        case PropertyRegistry::ParserHook::parse_length:
        case PropertyRegistry::ParserHook::parse_number_auto:
        case PropertyRegistry::ParserHook::parse_text_decoration:
        case PropertyRegistry::ParserHook::parse_font_family:
        case PropertyRegistry::ParserHook::parse_font_weight:
        case PropertyRegistry::ParserHook::parse_color:
        case PropertyRegistry::ParserHook::parse_opacity:
        // Grid hooks are handled before value-list parsing (see the interception
        // near the top of consume_declaration); listed here for switch
        // exhaustiveness.
        case PropertyRegistry::ParserHook::parse_grid_track_list:
        case PropertyRegistry::ParserHook::parse_grid_placement:
        case PropertyRegistry::ParserHook::parse_gap:
        case PropertyRegistry::ParserHook::Count:
            break;
    }

    Value value = values.empty() ? Value::identifier("") : values.front();
    push_decl(property, value);
    return true;
}

// Parses a @media prelude (tokens up to '{') into a width/height condition.
// Returns nullopt for anything we cannot evaluate (print, not, comma lists,
// non-px units, unknown features) so the caller skips the block conservatively.
// Note: the tokenizer drops parentheses, so "(min-width: 590px)" arrives as
// [min-width][:][590][px].
std::optional<MediaCondition> Parser::parse_media_prelude() {
    MediaCondition condition;
    while (!eof() && peek().type != TokenType::LBrace && peek().type != TokenType::Semicolon) {
        if (peek().type == TokenType::Whitespace) {
            advance();
            continue;
        }
        if (peek().type == TokenType::Identifier) {
            std::string_view word = peek().lexeme;
            if (word == "only" || word == "screen" || word == "all" || word == "and") {
                advance();
                continue;
            }
            const bool is_min_width = word == "min-width";
            const bool is_max_width = word == "max-width";
            const bool is_min_height = word == "min-height";
            const bool is_max_height = word == "max-height";
            if (is_min_width || is_max_width || is_min_height || is_max_height) {
                advance();
                skip_whitespace_tokens();
                if (!match(TokenType::Colon)) {
                    return std::nullopt;
                }
                skip_whitespace_tokens();
                if (peek().type != TokenType::Number) {
                    return std::nullopt;
                }
                float value = 0.0f;
                {
                    std::string number_text(advance().lexeme);
                    value = std::strtof(number_text.c_str(), nullptr);
                }
                if (peek().type == TokenType::Identifier) {
                    if (peek().lexeme != ValueNames::Px) {
                        return std::nullopt;  // em/rem media lengths unsupported.
                    }
                    advance();
                }
                if (is_min_width) condition.min_width = value;
                if (is_max_width) condition.max_width = value;
                if (is_min_height) condition.min_height = value;
                if (is_max_height) condition.max_height = value;
                continue;
            }
            return std::nullopt;  // not/print/orientation/prefers-*/...
        }
        return std::nullopt;  // comma lists and anything else
    }
    return condition;
}

void Parser::skip_at_rule_block() {
    // Consume the prelude up to either a block or a statement terminator.
    while (!eof() && peek().type != TokenType::LBrace && peek().type != TokenType::Semicolon) {
        advance();
    }
    if (match(TokenType::Semicolon)) {
        return;  // Statement form (@import/@charset).
    }
    if (!match(TokenType::LBrace)) {
        return;
    }
    // Skip the whole block with balanced braces so nested rules never leak out
    // as unconditional top-level rules.
    int depth = 1;
    while (!eof() && depth > 0) {
        if (peek().type == TokenType::LBrace) {
            ++depth;
        } else if (peek().type == TokenType::RBrace) {
            --depth;
        }
        advance();
    }
}

void Parser::handle_at_rule(Stylesheet& sheet, const std::optional<MediaCondition>& enclosing_media) {
    advance();  // consume '@'
    std::string_view name;
    if (peek().type == TokenType::Identifier) {
        name = peek().lexeme;
    }

    if (name == "media") {
        advance();  // consume "media"
        const size_t prelude_start = m_pos;
        auto condition = parse_media_prelude();
        if (condition && match(TokenType::LBrace)) {
            // Intersect with any enclosing @media condition (nested blocks).
            if (enclosing_media) {
                auto tighten_min = [](std::optional<float>& into, const std::optional<float>& from) {
                    if (from && (!into || *from > *into)) into = *from;
                };
                auto tighten_max = [](std::optional<float>& into, const std::optional<float>& from) {
                    if (from && (!into || *from < *into)) into = *from;
                };
                tighten_min(condition->min_width, enclosing_media->min_width);
                tighten_max(condition->max_width, enclosing_media->max_width);
                tighten_min(condition->min_height, enclosing_media->min_height);
                tighten_max(condition->max_height, enclosing_media->max_height);
            }
            while (!eof() && peek().type != TokenType::RBrace) {
                if (!parse_one_rule(sheet, condition)) {
                    break;
                }
            }
            match(TokenType::RBrace);
            return;
        }
        // Unevaluable prelude: rewind to it and skip the block conservatively.
        m_pos = prelude_start;
        skip_at_rule_block();
        return;
    }

    if (name == "font-face") {
        advance();  // consume "font-face"
        skip_whitespace_tokens();
        if (match(TokenType::LBrace)) {
            parse_font_face(sheet);
        } else {
            skip_at_rule_block();
        }
        return;
    }

    // Deduped process-wide; kept separate from m_unknown_properties so at-rule
    // names do not pollute the stylesheet's unknown-property diagnostics.
    static Core::Utils::WarnOnce skipped_at_rules;
    if (skipped_at_rules.should_log(name.empty() ? "@" : name)) {
        HB_LOG_WARN("[parser] Skipping unsupported at-rule: @" << name);
    }
    skip_at_rule_block();
}

namespace {
// Lowercased format hint inferred from a font url's file extension. Mirrors the
// `format(...)` keywords so both paths feed the same loadability check.
std::string infer_font_format(std::string_view url) {
    // Trim query/fragment before looking at the extension.
    size_t cut = url.find_first_of("?#");
    if (cut != std::string_view::npos) {
        url = url.substr(0, cut);
    }
    size_t dot = url.rfind('.');
    if (dot == std::string_view::npos) {
        return {};
    }
    std::string ext = Core::Utils::to_lower(std::string(url.substr(dot + 1)));
    if (ext == "ttf" || ext == "ttc") return "truetype";
    if (ext == "otf") return "opentype";
    if (ext == "woff") return "woff";
    if (ext == "woff2") return "woff2";
    return ext;
}
}  // namespace

// Parses the body of an `@font-face { ... }` block (the opening brace is already
// consumed) and appends a FontFaceRule to the sheet. Only `font-family` and
// `src` are captured; other descriptors are skipped. `src` collects each url()
// with its format hint (explicit `format(...)` preferred, else inferred from the
// extension) so a later loadability check can pick a raw TTF/OTF over WOFF2.
void Parser::parse_font_face(Stylesheet& sheet) {
    FontFaceRule rule;
    while (!eof() && peek().type != TokenType::RBrace) {
        skip_whitespace_tokens();
        if (eof() || peek().type == TokenType::RBrace) {
            break;
        }
        if (peek().type != TokenType::Identifier) {
            advance();
            continue;
        }
        std::string descriptor = Core::Utils::to_lower(std::string(advance().lexeme));
        skip_whitespace_tokens();
        if (!match(TokenType::Colon)) {
            while (!eof() && peek().type != TokenType::Semicolon && peek().type != TokenType::RBrace) {
                advance();
            }
            match(TokenType::Semicolon);
            continue;
        }
        if (descriptor == "font-family") {
            // The tokenizer strips quotes, so a quoted family arrives as plain
            // identifiers; join multi-word names with single spaces, lowercased.
            std::string family;
            while (!eof() && peek().type != TokenType::Semicolon && peek().type != TokenType::RBrace) {
                if (peek().type == TokenType::Identifier) {
                    if (!family.empty()) family.push_back(' ');
                    family += Core::Utils::to_lower(std::string(advance().lexeme));
                } else {
                    advance();
                }
            }
            rule.family = std::move(family);
        } else if (descriptor == "src") {
            while (!eof() && peek().type != TokenType::Semicolon && peek().type != TokenType::RBrace) {
                if (peek().type == TokenType::Url) {
                    FontFaceSource source;
                    source.url = std::string(advance().lexeme);
                    source.format = infer_font_format(source.url);
                    rule.sources.push_back(std::move(source));
                } else if (peek().type == TokenType::Identifier &&
                           Core::Utils::to_lower(std::string(peek().lexeme)) == "format") {
                    advance();  // consume "format"
                    skip_whitespace_tokens();
                    // The parenthesized string tokenizes as a bare identifier
                    // (parens/quotes are dropped); attribute it to the last url.
                    if (peek().type == TokenType::Identifier && !rule.sources.empty()) {
                        rule.sources.back().format = Core::Utils::to_lower(std::string(advance().lexeme));
                    }
                } else {
                    advance();
                }
            }
        } else {
            while (!eof() && peek().type != TokenType::Semicolon && peek().type != TokenType::RBrace) {
                advance();
            }
        }
        match(TokenType::Semicolon);
    }
    match(TokenType::RBrace);
    if (!rule.family.empty() && !rule.sources.empty()) {
        sheet.font_faces.push_back(std::move(rule));
    }
}

bool Parser::parse_one_rule(Stylesheet& sheet, const std::optional<MediaCondition>& media) {
    skip_whitespace_tokens();
    if (eof() || peek().type == TokenType::RBrace) {
        return false;
    }
    if (peek().type == TokenType::At) {
        handle_at_rule(sheet, media);
        return true;
    }
    auto selectors = parse_selectors();
    if (!match(TokenType::LBrace)) {
        advance();
        return true;
    }
    auto declarations = parse_declarations();
    match(TokenType::RBrace);
    if (!selectors.empty()) {
        sheet.rules.push_back({std::move(selectors), std::move(declarations), media});
    }
    return true;
}

Stylesheet Parser::parse() {
    Stylesheet sheet;
    while (!eof()) {
        if (!parse_one_rule(sheet, std::nullopt)) {
            // Stray closing brace at top level; consume and continue.
            if (peek().type == TokenType::RBrace) {
                advance();
                continue;
            }
            break;
        }
    }
    sheet.unknown_properties = m_unknown_properties.seen();
    return sheet;
}

}  // namespace Hummingbird::Css
