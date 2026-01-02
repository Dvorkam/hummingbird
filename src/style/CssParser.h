#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "style/CssTokenizer.h"
#include "style/Stylesheet.h"

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

    Selector parse_selector();
    std::vector<Selector> parse_selectors();
    Property parse_property();
    Value parse_value();
    std::vector<Declaration> parse_declarations();
    Property parse_property_name(std::string_view name) const;
    Value parse_hash_value();
    Value parse_identifier_value();
    Value parse_number_value();
    bool consume_declaration(std::vector<Declaration>& decls);

    std::string m_buffer;
    std::vector<Token> m_tokens;
    size_t m_pos = 0;
};

}  // namespace Hummingbird::Css
