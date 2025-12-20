#pragma once

#include "html/HtmlToken.h"
#include <string_view>

namespace Hummingbird::Html {

class Tokenizer {
public:
    explicit Tokenizer(std::string_view input);
    Token next_token();

private:
    enum class State {
        Data,
        TagOpen,
        TagName,
        EndTagOpen,
        SelfClosingStartTag
    };

    char peek_char(size_t offset = 0) const;
    char consume_char();
    bool eof() const;
    void skip_whitespace();

    Token emit_error(std::string_view message);
    Token emit_character_data();
    Token emit_tag(bool is_end_tag, bool self_closing, std::string_view tag_name, const std::array<Attribute, 8>& attrs, size_t attr_count);
    void parse_tag_name(std::string_view& out_name);
    size_t parse_attributes(std::array<Attribute, 8>& attrs);

    std::string_view m_input;
    size_t m_pos = 0;
    State m_state = State::Data;
};

} // namespace Hummingbird::Html
