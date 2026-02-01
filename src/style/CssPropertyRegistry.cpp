#include "style/CssPropertyRegistry.h"

#include "style/CssPropertyNames.h"

namespace Hummingbird::Css::PropertyRegistry {

struct Mapping {
    std::string_view name;
    Property property;
};

static constexpr Mapping kMappings[] = {
    {PropertyNames::Display, Property::Display},
    {PropertyNames::Background, Property::Background},
    {PropertyNames::BackgroundImage, Property::BackgroundImage},
    {PropertyNames::BackgroundRepeat, Property::BackgroundRepeat},
    {PropertyNames::BackgroundPosition, Property::BackgroundPosition},
    {PropertyNames::BackgroundSize, Property::BackgroundSize},
    {PropertyNames::Border, Property::Border},
    {PropertyNames::BorderWidth, Property::BorderWidth},
    {PropertyNames::BorderColor, Property::BorderColor},
    {PropertyNames::BorderStyle, Property::BorderStyle},
    {PropertyNames::Margin, Property::Margin},
    {PropertyNames::MarginTop, Property::MarginTop},
    {PropertyNames::MarginRight, Property::MarginRight},
    {PropertyNames::MarginBottom, Property::MarginBottom},
    {PropertyNames::MarginLeft, Property::MarginLeft},
    {PropertyNames::Padding, Property::Padding},
    {PropertyNames::PaddingTop, Property::PaddingTop},
    {PropertyNames::PaddingRight, Property::PaddingRight},
    {PropertyNames::PaddingBottom, Property::PaddingBottom},
    {PropertyNames::PaddingLeft, Property::PaddingLeft},
    {PropertyNames::Width, Property::Width},
    {PropertyNames::Height, Property::Height},
    {PropertyNames::Color, Property::Color},
    {PropertyNames::BackgroundColor, Property::BackgroundColor},
    {PropertyNames::FontSize, Property::FontSize},
    {PropertyNames::LineHeight, Property::LineHeight},
    {PropertyNames::MaxWidth, Property::MaxWidth},
    {PropertyNames::TextAlign, Property::TextAlign},
    {PropertyNames::TextDecoration, Property::TextDecoration},
    {PropertyNames::WhiteSpace, Property::WhiteSpace},
    {PropertyNames::FontFamily, Property::FontFamily},
    {PropertyNames::FontWeight, Property::FontWeight},
    {PropertyNames::FontStyle, Property::FontStyle},
    {PropertyNames::Float, Property::Float},
};

Property parse_property_name(std::string_view name) {
    for (const auto& mapping : kMappings) {
        if (name == mapping.name) {
            return mapping.property;
        }
    }
    return Property::Unknown;
}

bool is_supported_property(std::string_view name) {
    return parse_property_name(name) != Property::Unknown;
}

}  // namespace Hummingbird::Css::PropertyRegistry
