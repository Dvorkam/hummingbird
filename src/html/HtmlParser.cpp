#include "html/HtmlParser.h"

namespace Hummingbird::Html {

Parser::Parser(ArenaAllocator& arena, std::string_view html) : m_tokenizer(html), m_arena(arena) {}

ArenaPtr<DOM::Node> Parser::parse() {
    auto root = make_arena_ptr<DOM::Element>(m_arena, "root");
    std::vector<DOM::Node*> open_elements;
    open_elements.push_back(root.get());

    auto is_void_element = [](std::string_view name) {
        return name == "meta" || name == "link" || name == "br" || name == "img" || name == "input";
    };

    while (true) {
        Token token = m_tokenizer.next_token();

        if (token.type == TokenType::EndOfFile || token.type == TokenType::Error) {
            break;
        }

        switch (token.type) {
            case TokenType::StartTag: {
                auto& tag_data = std::get<StartTagToken>(token.data);
                auto new_element = make_arena_ptr<DOM::Element>(m_arena, std::string(tag_data.name));

                DOM::Node* parent = open_elements.back();
                if (auto parent_el = dynamic_cast<DOM::Element*>(parent)) {
                    if (parent_el->get_tag_name() == "head" && tag_data.name == "body" && open_elements.size() >= 2) {
                        parent = open_elements[open_elements.size() - 2];
                    }
                }

                // Apply attributes to the element.
                for (size_t i = 0; i < tag_data.attribute_count; ++i) {
                    const auto& attr = tag_data.attributes[i];
                    static_cast<DOM::Element*>(new_element.get())->set_attribute(std::string(attr.name), std::string(attr.value));
                }

                parent->append_child(std::move(new_element));

                DOM::Node* appended = parent->get_children().back().get();
                if (!is_void_element(tag_data.name)) {
                    open_elements.push_back(appended);
                }
                break;
            }
            case TokenType::EndTag: {
                if (open_elements.size() > 1) { // Don't pop the root
                    open_elements.pop_back();
                }
                break;
            }
            case TokenType::CharacterData: {
                auto& char_data = std::get<CharacterDataToken>(token.data);
                if (!char_data.data.empty()) {
                    auto new_text = make_arena_ptr<DOM::Text>(m_arena, std::string(char_data.data));
                    open_elements.back()->append_child(std::move(new_text));
                }
                break;
            }
            default:
                break;
        }
    }

    return ArenaPtr<DOM::Node>(root.release());
}

} // namespace Hummingbird::Html
