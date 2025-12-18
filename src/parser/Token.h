#pragma once

#include <string_view>
#include <variant>

namespace Hummingbird::Parser {

    enum class TokenType {
        StartTag,
        EndTag,
        CharacterData,
        EndOfFile,
        Error
    };

    struct StartTagToken {
        std::string_view name;
        // Attributes will be added later
    };

    struct EndTagToken {
        std::string_view name;
    };

    struct CharacterDataToken {
        std::string_view data;
    };

    struct ErrorToken {
        std::string_view message;
    };

    using TokenData = std::variant<
        StartTagToken,
        EndTagToken,
        CharacterDataToken,
        ErrorToken
    >;

    struct Token {
        TokenType type;
        TokenData data;
    };

}
