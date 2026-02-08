#pragma once

#include <string_view>

namespace Hummingbird::Css::PropertyNames {

#define HB_CSS_PROPERTY(property_id, name_id, css_name, canonical_name, parser_hook, applier_hook, flags) \
    static constexpr std::string_view name_id = css_name;
#define HB_CSS_PROPERTY_ALIAS(property_id, name_id, css_name, canonical_name, parser_hook, applier_hook, flags) \
    static constexpr std::string_view name_id = css_name;
#include "style/registry/CssPropertyList.inl"
#undef HB_CSS_PROPERTY_ALIAS
#undef HB_CSS_PROPERTY

}  // namespace Hummingbird::Css::PropertyNames
