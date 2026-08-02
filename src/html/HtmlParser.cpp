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
    m_semantic_tags.clear();
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
    result.unsupported_tags = m_unsupported_tags.seen();
    return result;
}

void Parser::handle_start_tag(const StartTagToken& tag_data, ParseState& state) {
    std::string lowered_name = Core::Utils::to_lower(tag_data.name);
    maybe_close_list_item(state, lowered_name);
    maybe_close_paragraph(state, lowered_name);

    // HTML parsers treat SVG content as "foreign content". We currently do not build a full SVG DOM/render tree
    // (we decode/paint <svg> as a replaced element), so avoid emitting "unsupported HTML tag" warnings for SVG
    // element names like <rect>/<circle> within an <svg> subtree.
    const bool in_svg_subtree = [&]() -> bool {
        for (auto* node : state.open_elements) {
            auto* element = dynamic_cast<DOM::Element*>(node);
            if (element && element->get_tag_name() == Hummingbird::Html::TagNames::Svg) {
                return true;
            }
        }
        return false;
    }();

    auto new_element = DOM::DomFactory::create_element(m_arena, lowered_name);
    if (!new_element) {
        m_failed = true;
        return;
    }
    apply_attributes(*new_element, tag_data);

    DOM::Node* parent = select_parent(state, lowered_name);
    parent->append_child(std::move(new_element));

    DOM::Node* appended = parent->get_children().back().get();
    if (!in_svg_subtree) {
        track_unsupported_tag(lowered_name);
    }
    track_semantic_tag(lowered_name);
    // Resource links are read back off the ELEMENT, not out of the raw token
    // (T-HTML-ATTR-ENTITY-DECODE-1). `apply_attributes` decoded the character
    // references on the way in, and these lists are what the loader actually
    // fetches — reading the token instead left the DOM correct while every
    // fetched URL kept its `&amp;`, so a link navigated fine and the identical
    // URL as an <img> src 404'd. One decoded source of truth, no second decode.
    const auto* appended_element = dynamic_cast<const DOM::Element*>(appended);
    const auto attribute_of = [appended_element](std::string_view name) -> std::string_view {
        if (!appended_element) return {};
        const auto* value = appended_element->find_attribute(name);
        return value ? std::string_view{*value} : std::string_view{};
    };
    if (lowered_name == Hummingbird::Html::TagNames::Link) {
        auto rel = Core::Utils::to_lower(attribute_of(Hummingbird::Html::AttributeNames::Rel));
        auto href = attribute_of(Hummingbird::Html::AttributeNames::Href);
        if (rel == "stylesheet" && !href.empty()) {
            m_stylesheet_links.emplace_back(href);
        }
    }
    if (lowered_name == Hummingbird::Html::TagNames::Img) {
        auto src = attribute_of(Hummingbird::Html::AttributeNames::Src);
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
    if (lowered_name == Hummingbird::Html::TagNames::Textarea && should_push) {
        state.textarea_has_value = false;
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
    auto* parent_element = dynamic_cast<DOM::Element*>(parent);
    const std::string_view parent_tag = parent_element ? std::string_view{parent_element->get_tag_name()} : "";

    // Raw text (script) is literal: expanding `&amp;` here would rewrite the
    // source the JS engine later executes.
    if (TagMetadata::is_raw_text_tag(parent_tag)) {
        append_text_node(parent, char_data.data);
        return;
    }

    auto decoded = Utils::decode_named_entities(char_data.data);

    // A textarea's content IS its default value, not child text. Folding it into
    // the `value` attribute at parse time gives the control exactly one owner —
    // the same attribute <input> already uses — so focus, editing, painting, and
    // form submission all reuse the existing single-value path unchanged, and no
    // stray text node renders underneath the control.
    if (parent_element && parent_tag == Hummingbird::Html::TagNames::Textarea) {
        // Per the HTML spec a single newline directly after the start tag is a
        // serialization artifact and is dropped.
        std::string_view value{decoded};
        if (!state.textarea_has_value) {
            if (value.starts_with("\r\n")) {
                value.remove_prefix(2);
            } else if (value.starts_with("\n")) {
                value.remove_prefix(1);
            }
        }
        std::string combined;
        if (const auto* existing = parent_element->find_attribute(Hummingbird::Html::AttributeNames::Value)) {
            combined = *existing;
        }
        combined.append(value);
        parent_element->set_attribute(Hummingbird::Html::AttributeNames::Value, combined);
        state.textarea_has_value = true;
        return;
    }

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
        // Character references are decoded in attribute values, not only in text
        // (T-HTML-ATTR-ENTITY-DECODE-1). HTML *requires* a literal `&` to be
        // written `&amp;` inside an attribute, so skipping this made every URL
        // with an ampersand wrong: `href="/wiki/Sam_&amp;_Max"` requested the
        // entity verbatim and 404'd on a real article.
        //
        // Safe to reuse the text decoder here because it only accepts a
        // SEMICOLON-TERMINATED reference. That is what the spec's
        // ambiguous-ampersand rule protects in attributes — a legacy query
        // string like `?a=1&amp=2` must keep its literal `&amp`, and it does,
        // because there is no `;` to terminate it.
        element.set_attribute(attr.name, Utils::decode_named_entities(attr.value));
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
    if (m_unsupported_tags.should_log(tag_name)) {
        HB_LOG_WARN("[parser] Unsupported HTML Tag encountered: <" << tag_name << ">");
    }
}

void Parser::track_semantic_tag(std::string_view tag_name) {
    if (!TagMetadata::is_semantic_block_tag(tag_name)) return;
    std::string name(tag_name);
    if (m_semantic_tags.insert(name).second) {
        HB_LOG_DEBUG("[parser] Semantic tag mapped to block layout (semantics not implemented): <" << name << ">");
    }
}

void Parser::maybe_close_paragraph(ParseState& state, std::string_view tag_name) {
    if (!TagMetadata::closes_paragraph_on_start(tag_name)) return;
    if (state.open_elements.size() <= 1) return;
    for (size_t i = state.open_elements.size(); i-- > 1;) {
        auto* element = dynamic_cast<DOM::Element*>(state.open_elements[i]);
        if (element && element->get_tag_name() == Hummingbird::Html::TagNames::P) {
            state.open_elements.resize(i);
            break;
        }
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
