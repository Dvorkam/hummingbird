#pragma once

#include <string_view>

#include "html/HtmlTagNames.h"

namespace Hummingbird::Html::TagMetadata {

inline bool is_void_tag(std::string_view name) {
    static constexpr std::string_view kVoidTags[] = {
        Hummingbird::Html::TagNames::Meta, Hummingbird::Html::TagNames::Link,  Hummingbird::Html::TagNames::Br,
        Hummingbird::Html::TagNames::Img,  Hummingbird::Html::TagNames::Input, Hummingbird::Html::TagNames::Hr};
    for (auto tag : kVoidTags) {
        if (tag == name) return true;
    }
    return false;
}

inline bool is_supported_tag(std::string_view name) {
    static constexpr std::string_view kKnownTags[] = {
        Hummingbird::Html::TagNames::Html,       Hummingbird::Html::TagNames::Head,
        Hummingbird::Html::TagNames::Body,       Hummingbird::Html::TagNames::Title,
        Hummingbird::Html::TagNames::Style,      Hummingbird::Html::TagNames::Script,
        Hummingbird::Html::TagNames::Div,        Hummingbird::Html::TagNames::P,
        Hummingbird::Html::TagNames::Span,       Hummingbird::Html::TagNames::H1,
        Hummingbird::Html::TagNames::H2,         Hummingbird::Html::TagNames::H3,
        Hummingbird::Html::TagNames::H4,         Hummingbird::Html::TagNames::H5,
        Hummingbird::Html::TagNames::H6,         Hummingbird::Html::TagNames::B,
        Hummingbird::Html::TagNames::Strong,     Hummingbird::Html::TagNames::I,
        Hummingbird::Html::TagNames::Em,         Hummingbird::Html::TagNames::Img,
        Hummingbird::Html::TagNames::Br,         Hummingbird::Html::TagNames::Hr,
        Hummingbird::Html::TagNames::Input,      Hummingbird::Html::TagNames::Button,
        Hummingbird::Html::TagNames::Form,       Hummingbird::Html::TagNames::Ul,
        Hummingbird::Html::TagNames::Ol,         Hummingbird::Html::TagNames::Li,
        Hummingbird::Html::TagNames::Dl,         Hummingbird::Html::TagNames::Dt,
        Hummingbird::Html::TagNames::Dd,         Hummingbird::Html::TagNames::Pre,
        Hummingbird::Html::TagNames::Code,       Hummingbird::Html::TagNames::A,
        Hummingbird::Html::TagNames::Blockquote, Hummingbird::Html::TagNames::Font,
        Hummingbird::Html::TagNames::Header,     Hummingbird::Html::TagNames::Nav,
        Hummingbird::Html::TagNames::Main,       Hummingbird::Html::TagNames::Section,
        Hummingbird::Html::TagNames::Article,    Hummingbird::Html::TagNames::Aside,
        Hummingbird::Html::TagNames::Footer,     Hummingbird::Html::TagNames::Meta,
        Hummingbird::Html::TagNames::Link,       Hummingbird::Html::TagNames::Table,
        Hummingbird::Html::TagNames::Thead,      Hummingbird::Html::TagNames::Tbody,
        Hummingbird::Html::TagNames::Tfoot,      Hummingbird::Html::TagNames::Tr,
        Hummingbird::Html::TagNames::Td,         Hummingbird::Html::TagNames::Th};
    for (auto tag : kKnownTags) {
        if (tag == name) return true;
    }
    return false;
}

inline bool is_known_tag(std::string_view name) {
    return is_supported_tag(name);
}

inline bool is_semantic_block_tag(std::string_view name) {
    static constexpr std::string_view kSemanticTags[] = {
        Hummingbird::Html::TagNames::Header,  Hummingbird::Html::TagNames::Nav,     Hummingbird::Html::TagNames::Main,
        Hummingbird::Html::TagNames::Section, Hummingbird::Html::TagNames::Article, Hummingbird::Html::TagNames::Aside,
        Hummingbird::Html::TagNames::Footer,
    };
    for (auto tag : kSemanticTags) {
        if (tag == name) return true;
    }
    return false;
}

inline bool closes_paragraph_on_start(std::string_view name) {
    static constexpr std::string_view kParagraphClosingTags[] = {
        Hummingbird::Html::TagNames::P,       Hummingbird::Html::TagNames::Div,
        Hummingbird::Html::TagNames::Dl,      Hummingbird::Html::TagNames::Dt,
        Hummingbird::Html::TagNames::Dd,      Hummingbird::Html::TagNames::Ul,
        Hummingbird::Html::TagNames::Ol,      Hummingbird::Html::TagNames::Li,
        Hummingbird::Html::TagNames::Pre,     Hummingbird::Html::TagNames::Blockquote,
        Hummingbird::Html::TagNames::Table,   Hummingbird::Html::TagNames::Thead,
        Hummingbird::Html::TagNames::Tbody,   Hummingbird::Html::TagNames::Tfoot,
        Hummingbird::Html::TagNames::Tr,      Hummingbird::Html::TagNames::Td,
        Hummingbird::Html::TagNames::Th,      Hummingbird::Html::TagNames::Form,
        Hummingbird::Html::TagNames::Hr,      Hummingbird::Html::TagNames::H1,
        Hummingbird::Html::TagNames::H2,      Hummingbird::Html::TagNames::H3,
        Hummingbird::Html::TagNames::H4,      Hummingbird::Html::TagNames::H5,
        Hummingbird::Html::TagNames::H6,      Hummingbird::Html::TagNames::Header,
        Hummingbird::Html::TagNames::Nav,     Hummingbird::Html::TagNames::Main,
        Hummingbird::Html::TagNames::Section, Hummingbird::Html::TagNames::Article,
        Hummingbird::Html::TagNames::Aside,   Hummingbird::Html::TagNames::Footer,
    };
    for (auto tag : kParagraphClosingTags) {
        if (tag == name) return true;
    }
    return false;
}

}  // namespace Hummingbird::Html::TagMetadata
