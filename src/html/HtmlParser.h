#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "core/ArenaAllocator.h"
#include "core/dom/Element.h"
#include "core/dom/Node.h"
#include "core/dom/Text.h"
#include "html/HtmlTokenizer.h"

namespace Hummingbird::Html {

class Parser {
public:
    Parser(ArenaAllocator& arena, std::string_view html);
    ArenaPtr<DOM::Node> parse();
    const std::unordered_set<std::string>& unsupported_tags() const { return m_unsupported_tags; }
    const std::vector<std::string>& style_blocks() const { return m_style_blocks; }

private:
    Tokenizer m_tokenizer;
    ArenaAllocator& m_arena;
    std::unordered_set<std::string> m_unsupported_tags;
    std::vector<std::string> m_style_blocks;
};

}  // namespace Hummingbird::Html
