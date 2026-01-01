#include "html/HtmlParser.h"

#include <algorithm>
#include <cctype>

#include "core/dom/DomFactory.h"
#include "core/utils/Log.h"
#include "html/HtmlTagNames.h"

namespace Hummingbird::Html {

Parser::Parser(ArenaAllocator& arena, std::string_view html) : m_tokenizer(html), m_arena(arena) {}

namespace {
std::string to_lower(const std::string_view& view) {
    std::string out(view);
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

bool iequals(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

std::string_view find_attribute(const StartTagToken& tag_data, std::string_view name) {
    for (size_t i = 0; i < tag_data.attribute_count; ++i) {
        const auto& attr = tag_data.attributes[i];
        if (iequals(attr.name, name)) {
            return attr.value;
        }
    }
    return {};
}

bool is_void_element(std::string_view name) {
    static constexpr std::string_view kVoidElements[] = {
        Hummingbird::Html::TagNames::Meta, Hummingbird::Html::TagNames::Link,  Hummingbird::Html::TagNames::Br,
        Hummingbird::Html::TagNames::Img,  Hummingbird::Html::TagNames::Input, Hummingbird::Html::TagNames::Hr};
    for (auto tag : kVoidElements) {
        if (tag == name) return true;
    }
    return false;
}

bool is_known_element(std::string_view name) {
    static constexpr std::string_view kKnown[] = {
        Hummingbird::Html::TagNames::Html,   Hummingbird::Html::TagNames::Head,
        Hummingbird::Html::TagNames::Body,   Hummingbird::Html::TagNames::Title,
        Hummingbird::Html::TagNames::Style,  Hummingbird::Html::TagNames::Script,
        Hummingbird::Html::TagNames::Div,    Hummingbird::Html::TagNames::P,
        Hummingbird::Html::TagNames::Span,   Hummingbird::Html::TagNames::H1,
        Hummingbird::Html::TagNames::H2,     Hummingbird::Html::TagNames::H3,
        Hummingbird::Html::TagNames::H4,     Hummingbird::Html::TagNames::H5,
        Hummingbird::Html::TagNames::H6,     Hummingbird::Html::TagNames::B,
        Hummingbird::Html::TagNames::Strong, Hummingbird::Html::TagNames::I,
        Hummingbird::Html::TagNames::Em,     Hummingbird::Html::TagNames::Img,
        Hummingbird::Html::TagNames::Br,     Hummingbird::Html::TagNames::Hr,
        Hummingbird::Html::TagNames::Input,  Hummingbird::Html::TagNames::Ul,
        Hummingbird::Html::TagNames::Ol,     Hummingbird::Html::TagNames::Li,
        Hummingbird::Html::TagNames::Pre,    Hummingbird::Html::TagNames::Code,
        Hummingbird::Html::TagNames::A,      Hummingbird::Html::TagNames::Blockquote,
        Hummingbird::Html::TagNames::Meta,   Hummingbird::Html::TagNames::Link};
    for (auto tag : kKnown) {
        if (tag == name) return true;
    }
    return false;
}
}  // namespace

Parser::Result Parser::parse() {
    m_style_blocks.clear();
    m_stylesheet_links.clear();
    m_unsupported_tags.clear();
    auto root = DOM::DomFactory::create_element(m_arena, Hummingbird::Html::TagNames::Root);
    ParseState state;
    state.open_elements.push_back(root.get());

    while (true) {
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

    Result result;
    result.dom = ArenaPtr<DOM::Node>(root.release());
    result.style_blocks = std::move(m_style_blocks);
    result.stylesheet_links = std::move(m_stylesheet_links);
    result.unsupported_tags = std::move(m_unsupported_tags);
    return result;
}

void Parser::handle_start_tag(const StartTagToken& tag_data, ParseState& state) {
    std::string lowered_name = to_lower(tag_data.name);
    maybe_close_list_item(state, lowered_name);

    auto new_element = DOM::DomFactory::create_element(m_arena, lowered_name);
    apply_attributes(*new_element, tag_data);

    DOM::Node* parent = select_parent(state, lowered_name);
    parent->append_child(std::move(new_element));

    DOM::Node* appended = parent->get_children().back().get();
    track_unsupported_tag(lowered_name);
    if (lowered_name == Hummingbird::Html::TagNames::Link) {
        auto rel = to_lower(find_attribute(tag_data, "rel"));
        auto href = find_attribute(tag_data, "href");
        if (rel == "stylesheet" && !href.empty()) {
            m_stylesheet_links.emplace_back(href);
        }
    }

    bool should_push = !is_void_element(lowered_name) && !tag_data.self_closing;
    if (should_push) {
        state.open_elements.push_back(appended);
    }
    if (lowered_name == Hummingbird::Html::TagNames::Style && should_push) {
        m_style_blocks.emplace_back();
        state.in_style = true;
    }
}

void Parser::handle_end_tag(const EndTagToken& end_data, ParseState& state) {
    std::string lowered_end = to_lower(end_data.name);
    if (lowered_end == Hummingbird::Html::TagNames::Style) {
        state.in_style = false;
    }
    pop_to_matching_ancestor(state, lowered_end);
}

void Parser::handle_character_data(const CharacterDataToken& char_data, ParseState& state) {
    if (char_data.data.empty()) return;
    if (state.in_style && !m_style_blocks.empty()) {
        m_style_blocks.back().append(char_data.data);
    }
    DOM::Node* parent = state.open_elements.back();
    append_text_node(parent, char_data.data);
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
    for (size_t i = 0; i < tag_data.attribute_count; ++i) {
        const auto& attr = tag_data.attributes[i];
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
    parent->append_child(std::move(new_text));
}

void Parser::track_unsupported_tag(std::string_view tag_name) {
    if (is_known_element(tag_name)) return;
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
