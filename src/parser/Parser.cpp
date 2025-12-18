#include "parser/Parser.h"
#include "parser/Tokenizer.h"
#include "core/dom/Element.h"
#include "core/dom/Text.h"
#include <vector>

namespace Hummingbird::Parser {

Parser::Parser(std::string_view html) : m_tokenizer(html) {}

std::unique_ptr<DOM::Node> Parser::parse() {
    // For now, we'll create a dummy root node. In a real scenario,
    // this might be a Document node.
    auto root = std::make_unique<DOM::Element>("root");
    std::vector<DOM::Node*> open_elements;
    open_elements.push_back(root.get());

    while(true) {
        Token token = m_tokenizer.next_token();

        if (token.type == TokenType::EndOfFile || token.type == TokenType::Error) {
            break;
        }

        switch (token.type) {
            case TokenType::StartTag: {
                auto& tag_data = std::get<StartTagToken>(token.data);
                auto new_element = std::make_unique<DOM::Element>(std::string(tag_data.name));
                
                DOM::Node* parent = open_elements.back();
                parent->append_child(std::move(new_element));

                // The raw pointer is safe because the unique_ptr is owned by the parent.
                open_elements.push_back(parent->get_children().back().get());
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
                    auto new_text = std::make_unique<DOM::Text>(std::string(char_data.data));
                    open_elements.back()->append_child(std::move(new_text));
                }
                break;
            }
            default:
                break;
        }
    }

    return root;
}

// Private member declaration needed in the header
// Tokenizer m_tokenizer;

} // namespace Hummingbird::Parser
