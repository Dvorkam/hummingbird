#pragma once

#include <string_view>
#include <variant>
#include <array>

namespace Hummingbird::Html {

enum class TokenType {
    StartTag,
    EndTag,
    CharacterData,
    EndOfFile,
    Error
};

struct Attribute {
    std::string_view name;
    std::string_view value;
};

struct StartTagToken {
    std::string_view name;
    std::array<Attribute, 8> attributes{};
    size_t attribute_count{0};
    bool self_closing{false};
};
struct EndTagToken { std::string_view name; };
struct CharacterDataToken { std::string_view data; };
struct ErrorToken { std::string_view message; };
struct EndOfFileToken {};

struct Token {
    TokenType type;
    std::variant<StartTagToken, EndTagToken, CharacterDataToken, ErrorToken, EndOfFileToken> data;
};

} // namespace Hummingbird::Html
