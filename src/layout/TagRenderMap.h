#pragma once

#include <string_view>

#include "core/dom/Element.h"
#include "html/HtmlTagNames.h"
#include "layout/RenderFactory.h"

namespace Hummingbird::Layout::TagRenderMap {

inline bool is_non_visual_tag(std::string_view tag) {
    static constexpr std::string_view kNonVisualTags[] = {
        Hummingbird::Html::TagNames::Head, Hummingbird::Html::TagNames::Style, Hummingbird::Html::TagNames::Title,
        Hummingbird::Html::TagNames::Script};
    for (auto candidate : kNonVisualTags) {
        if (tag == candidate) {
            return true;
        }
    }
    return false;
}

using CreateFn = std::unique_ptr<RenderObject> (*)(const DOM::Element*);

inline std::unique_ptr<RenderObject> create_break_render(const DOM::Element* element) {
    return RenderFactory::create_break(element);
}

inline std::unique_ptr<RenderObject> create_rule_render(const DOM::Element* element) {
    return RenderFactory::create_rule(element);
}

inline std::unique_ptr<RenderObject> create_table_render(const DOM::Element* element) {
    return RenderFactory::create_table(element);
}

inline std::unique_ptr<RenderObject> create_table_section_render(const DOM::Element* element) {
    return RenderFactory::create_table_section(element);
}

inline std::unique_ptr<RenderObject> create_table_row_render(const DOM::Element* element) {
    return RenderFactory::create_table_row(element);
}

inline std::unique_ptr<RenderObject> create_table_cell_render(const DOM::Element* element) {
    return RenderFactory::create_table_cell(element);
}

struct Mapping {
    std::string_view tag;
    CreateFn create;
};

inline constexpr Mapping kSpecialMappings[] = {
    {Hummingbird::Html::TagNames::Br, &create_break_render},
    {Hummingbird::Html::TagNames::Hr, &create_rule_render},
    {Hummingbird::Html::TagNames::Img, &RenderFactory::create_image},
    {Hummingbird::Html::TagNames::Table, &create_table_render},
    {Hummingbird::Html::TagNames::Thead, &create_table_section_render},
    {Hummingbird::Html::TagNames::Tbody, &create_table_section_render},
    {Hummingbird::Html::TagNames::Tfoot, &create_table_section_render},
    {Hummingbird::Html::TagNames::Tr, &create_table_row_render},
    {Hummingbird::Html::TagNames::Td, &create_table_cell_render},
    {Hummingbird::Html::TagNames::Th, &create_table_cell_render},
};

inline bool is_special_tag(std::string_view tag) {
    for (const auto& mapping : kSpecialMappings) {
        if (tag == mapping.tag) {
            return true;
        }
    }
    return false;
}

inline std::unique_ptr<RenderObject> create_special_render_object(const DOM::Element* element) {
    if (!element) {
        return nullptr;
    }

    const auto& tag = element->get_tag_name();
    for (const auto& mapping : kSpecialMappings) {
        if (tag == mapping.tag) {
            return mapping.create(element);
        }
    }
    return nullptr;
}

}  // namespace Hummingbird::Layout::TagRenderMap
