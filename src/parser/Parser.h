#pragma once

#include "core/dom/Node.h"
#include "parser/Tokenizer.h"
#include <string_view>
#include <memory>

namespace Hummingbird::Parser {

    class Parser {
    public:
        Parser(std::string_view html);

        std::unique_ptr<DOM::Node> parse();

    private:
        Tokenizer m_tokenizer;
    };

}
