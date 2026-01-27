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
#include "core/utils/WarnOnce.h"
#include "html/HtmlTokenizer.h"

namespace Hummingbird {
namespace DOM {
class Element;
}  // namespace DOM
namespace Html {
struct CharacterDataToken;
struct EndTagToken;
struct StartTagToken;
}  // namespace Html
}  // namespace Hummingbird

namespace Hummingbird::Html {

class Parser {
public:
    Parser(Core::ArenaAllocator& arena, std::string_view html);
    struct Result {
        Core::ArenaPtr<DOM::Node> dom;
        std::vector<std::string> style_blocks;
        std::vector<std::string> stylesheet_links;
        std::vector<std::string> image_links;
        std::unordered_set<std::string> unsupported_tags;
    };

    Result parse();

private:
    struct ParseState {
        std::vector<DOM::Node*> open_elements;
        bool in_style = false;
    };

    void handle_start_tag(const StartTagToken& tag_data, ParseState& state);
    void handle_end_tag(const EndTagToken& end_data, ParseState& state);
    void handle_character_data(const CharacterDataToken& char_data, ParseState& state);

    DOM::Node* select_parent(const ParseState& state, std::string_view tag_name) const;
    void apply_attributes(DOM::Element& element, const StartTagToken& tag_data);
    void append_text_node(DOM::Node* parent, std::string_view text);
    void track_unsupported_tag(std::string_view tag_name);
    void track_semantic_tag(std::string_view tag_name);
    void maybe_close_paragraph(ParseState& state, std::string_view tag_name);
    void pop_to_matching_ancestor(ParseState& state, std::string_view tag_name);
    void maybe_close_list_item(ParseState& state, std::string_view tag_name);

    Tokenizer m_tokenizer;
    Core::ArenaAllocator& m_arena;
    Core::Utils::WarnOnce m_unsupported_tags;
    std::unordered_set<std::string> m_semantic_tags;
    std::vector<std::string> m_style_blocks;
    std::vector<std::string> m_stylesheet_links;
    std::vector<std::string> m_image_links;
    bool m_failed = false;
};

}  // namespace Hummingbird::Html
