#pragma once

#include "html/HtmlTokenizer.h"
#include "core/dom/Node.h"
#include "core/dom/Element.h"
#include "core/dom/Text.h"
#include "core/ArenaAllocator.h"
#include <memory>
#include <string_view>

namespace Hummingbird::Html {

class Parser {
public:
    Parser(ArenaAllocator& arena, std::string_view html);
    ArenaPtr<DOM::Node> parse();

private:
    Tokenizer m_tokenizer;
    ArenaAllocator& m_arena;
};

} // namespace Hummingbird::Html
