#include "html/HtmlParser.h"

#include <stddef.h>

#include <memory>
#include <ostream>
#include <utility>
#include <variant>

#include "core/dom/DomFactory.h"
#include "core/dom/Element.h"
#include "core/dom/Text.h"
#include "core/utils/Log.h"
#include "core/utils/StringUtils.h"
#include "html/HtmlAttributeNames.h"
#include "html/HtmlEntityDecoder.h"
#include "html/HtmlStringUtils.h"
#include "html/HtmlTagMetadata.h"
#include "html/HtmlTagNames.h"
#include "html/HtmlToken.h"

namespace Hummingbird::Html {

Parser::Parser(Core::ArenaAllocator& arena, std::string_view html) : m_tokenizer(html), m_arena(arena) {}

Parser::Result Parser::parse() {
    m_style_blocks.clear();
    m_stylesheet_links.clear();
    m_image_links.clear();
    m_unsupported_tags.clear();
    m_failed = false;
    auto root = DOM::DomFactory::create_element(m_arena, Hummingbird::Html::TagNames::Root);
    if (!root) {
        m_failed = true;
        return {};
    }
    ParseState state;
    state.open_elements.push_back(root.get());

    while (true) {
        if (m_failed) break;
        Token token = m_tokenizer.next_token();

        if (token.type == TokenType::EndOfFile || token.type == TokenType::Error) {
            break;
        }

        switch (token.type) {
            case TokenType::StartTag: {
                auto& tag_data = std::get<StartTagToken>(token.data);
                handle_start_tag(tag_data, state);
                break;
            }
            case TokenType::EndTag: {
                auto& end_data = std::get<EndTagToken>(token.data);
                handle_end_tag(end_data, state);
                break;
            }
            case TokenType::CharacterData: {
                auto& char_data = std::get<CharacterDataToken>(token.data);
                handle_character_data(char_data, state);
                break;
            }
            default:
                break;
        }
    }

    if (m_failed) {
        return {};
    }

    Result result;
    result.dom = Core::ArenaPtr<DOM::Node>(root.release());
    result.style_blocks = std::move(m_style_blocks);
    result.stylesheet_links = std::move(m_stylesheet_links);
    result.image_links = std::move(m_image_links);
    result.unsupported_tags = std::move(m_unsupported_tags);
    return result;
}

void Parser::handle_start_tag(const StartTagToken& tag_data, ParseState& state) {
    std::string lowered_name = Core::Utils::to_lower(tag_data.name);
    maybe_close_list_item(state, lowered_name);

    auto new_element = DOM::DomFactory::create_element(m_arena, lowered_name);
    if (!new_element) {
        m_failed = true;
        return;
    }
    apply_attributes(*new_element, tag_data);

    DOM::Node* parent = select_parent(state, lowered_name);
    parent->append_child(std::move(new_element));

    DOM::Node* appended = parent->get_children().back().get();
    track_unsupported_tag(lowered_name);
    if (lowered_name == Hummingbird::Html::TagNames::Link) {
        auto rel = Core::Utils::to_lower(Utils::find_attribute(tag_data, Hummingbird::Html::AttributeNames::Rel));
        auto href = Utils::find_attribute(tag_data, Hummingbird::Html::AttributeNames::Href);
        if (rel == "stylesheet" && !href.empty()) {
            m_stylesheet_links.emplace_back(href);
        }
    }
    if (lowered_name == Hummingbird::Html::TagNames::Img) {
        auto src = Utils::find_attribute(tag_data, Hummingbird::Html::AttributeNames::Src);
        if (!src.empty()) {
            m_image_links.emplace_back(src);
        }
    }

    bool should_push = !TagMetadata::is_void_tag(lowered_name) && !tag_data.self_closing;
    if (should_push) {
        state.open_elements.push_back(appended);
    }
    if (lowered_name == Hummingbird::Html::TagNames::Style && should_push) {
        m_style_blocks.emplace_back();
        state.in_style = true;
    }
}

void Parser::handle_end_tag(const EndTagToken& end_data, ParseState& state) {
    std::string lowered_end = Core::Utils::to_lower(end_data.name);
    if (lowered_end == Hummingbird::Html::TagNames::Style) {
        state.in_style = false;
    }
    pop_to_matching_ancestor(state, lowered_end);
}

void Parser::handle_character_data(const CharacterDataToken& char_data, ParseState& state) {
    if (char_data.data.empty()) return;
    if (state.in_style && !m_style_blocks.empty()) {
        m_style_blocks.back().append(char_data.data);
        return;
    }
    DOM::Node* parent = state.open_elements.back();
    auto decoded = Utils::decode_named_entities(char_data.data);
    append_text_node(parent, decoded);
}

DOM::Node* Parser::select_parent(const ParseState& state, std::string_view tag_name) const {
    DOM::Node* parent = state.open_elements.back();
    if (auto parent_el = dynamic_cast<DOM::Element*>(parent)) {
        if (parent_el->get_tag_name() == Hummingbird::Html::TagNames::Head &&
            tag_name == Hummingbird::Html::TagNames::Body && state.open_elements.size() >= 2) {
            return state.open_elements[state.open_elements.size() - 2];
        }
    }
    return parent;
}

void Parser::apply_attributes(DOM::Element& element, const StartTagToken& tag_data) {
    for (const auto& attr : tag_data.attributes) {
        element.set_attribute(attr.name, attr.value);
    }
}

void Parser::append_text_node(DOM::Node* parent, std::string_view text) {
    auto& children = parent->get_children();
    if (!children.empty()) {
        if (auto* last_text = dynamic_cast<DOM::Text*>(children.back().get())) {
            last_text->append(text);
            return;
        }
    }
    auto new_text = DOM::DomFactory::create_text(m_arena, text);
    if (!new_text) {
        m_failed = true;
        return;
    }
    parent->append_child(std::move(new_text));
}

void Parser::track_unsupported_tag(std::string_view tag_name) {
    if (TagMetadata::is_supported_tag(tag_name)) return;
    std::string name(tag_name);
    if (m_unsupported_tags.insert(name).second) {
        HB_LOG_WARN("[parser] Unsupported HTML Tag encountered: <" << name << ">");
    }
}

void Parser::pop_to_matching_ancestor(ParseState& state, std::string_view tag_name) {
    if (state.open_elements.size() <= 1) return;
    size_t match_index = 0;
    bool found = false;
    for (size_t i = state.open_elements.size(); i-- > 1;) {  // skip root at 0
        auto* element = dynamic_cast<DOM::Element*>(state.open_elements[i]);
        if (element && element->get_tag_name() == tag_name) {
            match_index = i;
            found = true;
            break;
        }
    }
    if (found) {
        state.open_elements.resize(match_index);
    }
}

void Parser::maybe_close_list_item(ParseState& state, std::string_view tag_name) {
    if (tag_name != Hummingbird::Html::TagNames::Li || state.open_elements.empty()) return;
    if (auto* top_el = dynamic_cast<DOM::Element*>(state.open_elements.back())) {
        if (top_el->get_tag_name() == Hummingbird::Html::TagNames::Li) {
            state.open_elements.pop_back();
        }
    }
}

}  // namespace Hummingbird::Html
