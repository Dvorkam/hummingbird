#pragma once

#include <stddef.h>

#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "core/utils/WarnOnce.h"
#include "style/compute/Stylesheet.h"
#include "style/parser/CssTokenizer.h"

namespace Hummingbird::Css {

class Parser {
public:
    explicit Parser(std::string_view input);
    Stylesheet parse();

private:
    const Token& peek() const;
    const Token& advance();
    bool match(TokenType type);
    bool eof() const;

    void skip_whitespace_tokens();
    Selector parse_selector();
    SelectorPart parse_simple_selector();
    std::vector<Selector> parse_selectors();
    Property parse_property();
    Value parse_value();
    std::vector<Value> parse_value_list(bool* important = nullptr);
    bool consume_important_flag();
    std::vector<Declaration> parse_declarations();
    Property parse_property_name(std::string_view name) const;
    Value parse_hash_value();
    Value parse_identifier_value();
    std::optional<Color> parse_color_function();
    std::optional<Value::Calc> parse_calc();
    Value parse_number_value();
    std::string parse_font_family_list(bool* important = nullptr);
    std::string parse_custom_property_value(bool* important = nullptr);
    bool consume_declaration(std::vector<Declaration>& decls);
    void handle_at_rule(Stylesheet& sheet, const std::optional<MediaCondition>& enclosing_media);
    void parse_font_face(Stylesheet& sheet);
    void skip_at_rule_block();
    std::optional<MediaCondition> parse_media_prelude();
    bool parse_one_rule(Stylesheet& sheet, const std::optional<MediaCondition>& media);

    std::string m_buffer;
    std::vector<Token> m_tokens;
    size_t m_pos = 0;
    Core::Utils::WarnOnce m_unknown_properties;
};

}  // namespace Hummingbird::Css
