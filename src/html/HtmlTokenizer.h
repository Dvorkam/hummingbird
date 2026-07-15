#pragma once

#include <stddef.h>

#include <string_view>
#include <vector>

#include "html/HtmlToken.h"

namespace Hummingbird::Html {

class Tokenizer {
public:
    explicit Tokenizer(std::string_view input);
    Token next_token();

private:
    enum class State { Data, TagOpen, TagName, EndTagOpen, SelfClosingStartTag, RawText };

    char peek_char(size_t offset = 0) const;
    char consume_char();
    bool eof() const;
    void skip_whitespace();

    Token emit_character_data();
    Token emit_tag(bool is_end_tag, bool self_closing, std::string_view tag_name, std::vector<Attribute> attrs);
    void parse_tag_name(std::string_view& out_name);
    void parse_attributes(std::vector<Attribute>& attrs);
    bool handle_data_state(Token& out);
    bool handle_tag_open_state(Token& out);
    bool handle_end_tag_open_state(Token& out);
    bool handle_rawtext_state(Token& out);
    void skip_directive_or_comment();
    void skip_until(char terminal);

    std::string_view m_input;
    size_t m_pos = 0;
    State m_state = State::Data;
    // The tag name (script/style) whose matching end tag terminates RawText.
    std::string_view m_rawtext_tag;
};

}  // namespace Hummingbird::Html
