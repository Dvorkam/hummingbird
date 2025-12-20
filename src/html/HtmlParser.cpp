#include "html/HtmlParser.h"

#include <algorithm>

#include "core/Log.h"

namespace Hummingbird::Html {

Parser::Parser(ArenaAllocator& arena, std::string_view html) : m_tokenizer(html), m_arena(arena) {}

ArenaPtr<DOM::Node> Parser::parse() {
    auto root = make_arena_ptr<DOM::Element>(m_arena, "root");
    std::vector<DOM::Node*> open_elements;
    open_elements.push_back(root.get());

    auto is_void_element = [](std::string_view name) {
        return name == "meta" || name == "link" || name == "br" || name == "img" || name == "input" || name == "hr";
    };

    auto is_known_element = [&](std::string_view name) {
        static const std::vector<std::string_view> known = {"html", "head", "body", "title", "style", "script",
                                                            "div",  "p",    "span", "h1",    "b",     "strong",
                                                            "i",    "em",   "img",  "br",    "hr",    "input"};
        return std::find(known.begin(), known.end(), name) != known.end();
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
                    static_cast<DOM::Element*>(new_element.get())
                        ->set_attribute(std::string(attr.name), std::string(attr.value));
                }

                parent->append_child(std::move(new_element));

                DOM::Node* appended = parent->get_children().back().get();
                if (!is_known_element(tag_data.name)) {
                    std::string tag_name(tag_data.name);
                    if (m_unsupported_tags.insert(tag_name).second) {
                        HB_LOG_WARN("[parser] Unsupported HTML Tag encountered: <" << tag_name << ">");
                    }
                }

                bool should_push = !is_void_element(tag_data.name) && !tag_data.self_closing;
                if (should_push) {
                    open_elements.push_back(appended);
                }
                break;
            }
            case TokenType::EndTag: {
                auto& end_data = std::get<EndTagToken>(token.data);
                if (open_elements.size() > 1) {  // Don't pop the root
                    // Find the nearest matching open element and pop everything above it.
                    size_t match_index = 0;
                    bool found = false;
                    for (size_t i = open_elements.size(); i-- > 1;) {  // skip root at 0
                        auto* element = dynamic_cast<DOM::Element*>(open_elements[i]);
                        if (element && element->get_tag_name() == end_data.name) {
                            match_index = i;
                            found = true;
                            break;
                        }
                    }
                    if (found) {
                        open_elements.resize(match_index);
                    }
                }
                break;
            }
            case TokenType::CharacterData: {
                auto& char_data = std::get<CharacterDataToken>(token.data);
                if (!char_data.data.empty()) {
                    DOM::Node* parent = open_elements.back();
                    auto& children = parent->get_children();
                    if (!children.empty()) {
                        if (auto* last_text = dynamic_cast<DOM::Text*>(children.back().get())) {
                            last_text->append(std::string(char_data.data));
                            break;
                        }
                    }
                    auto new_text = make_arena_ptr<DOM::Text>(m_arena, std::string(char_data.data));
                    parent->append_child(std::move(new_text));
                }
                break;
            }
            default:
                break;
        }
    }

    return ArenaPtr<DOM::Node>(root.release());
}

}  // namespace Hummingbird::Html
