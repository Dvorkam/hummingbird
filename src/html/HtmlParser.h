#pragma once

#include "html/HtmlTokenizer.h"
#include "core/dom/Node.h"
#include "core/dom/Element.h"
#include "core/dom/Text.h"
#include "core/ArenaAllocator.h"
#include <memory>
#include <string_view>
#include <unordered_set>
#include <string>

namespace Hummingbird::Html {

class Parser {
public:
    Parser(ArenaAllocator& arena, std::string_view html);
    ArenaPtr<DOM::Node> parse();
    const std::unordered_set<std::string>& unsupported_tags() const { return m_unsupported_tags; }

private:
    Tokenizer m_tokenizer;
    ArenaAllocator& m_arena;
    std::unordered_set<std::string> m_unsupported_tags;
};

} // namespace Hummingbird::Html
