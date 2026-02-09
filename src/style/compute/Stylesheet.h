#pragma once

#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "core/platform_api/IGraphicsContext.h"

namespace Hummingbird::Css {

enum class Property {
    Unknown,
    Custom,
#define HB_CSS_PROPERTY(property_id, name_id, css_name, canonical_name, parser_hook, applier_hook, flags) property_id,
#define HB_CSS_PROPERTY_ALIAS(property_id, name_id, css_name, canonical_name, parser_hook, applier_hook, flags)
#include "style/registry/CssPropertyList.inl"
#undef HB_CSS_PROPERTY_ALIAS
#undef HB_CSS_PROPERTY
};

enum class Unit {
    Px,
    Em,
    Percent,
    Unknown,
};

struct Length {
    float value = 0.0f;
    Unit unit = Unit::Unknown;
};

struct Value {
    enum class Type {
        Identifier,
        Length,
        Color,
        Url,
        Number,
        Shadow,
    };

    struct Shadow {
        Length offset_x;
        Length offset_y;
        Length blur;
        Color color{0, 0, 0, 255};
    };

    Type type = Type::Identifier;
    std::string ident;
    Length length;
    Color color{0, 0, 0, 255};
    float number = 0.0f;
    Shadow shadow;

    static Value identifier(std::string text) {
        Value v;
        v.type = Type::Identifier;
        v.ident = std::move(text);
        return v;
    }

    static Value length_value(float value, Unit unit) {
        Value v;
        v.type = Type::Length;
        v.length = {value, unit};
        return v;
    }

    static Value color_value(Color color) {
        Value v;
        v.type = Type::Color;
        v.color = color;
        return v;
    }

    static Value url_value(std::string text) {
        Value v;
        v.type = Type::Url;
        v.ident = std::move(text);
        return v;
    }

    static Value number_value(float value) {
        Value v;
        v.type = Type::Number;
        v.number = value;
        return v;
    }

    static Value shadow_value(Shadow shadow) {
        Value v;
        v.type = Type::Shadow;
        v.shadow = shadow;
        return v;
    }
};

struct SelectorPart {
    enum class PseudoClass {
        Hover,
        Active,
        Focus,
    };

    std::string tag;
    std::string id;
    std::vector<std::string> classes;
    std::vector<PseudoClass> pseudo_classes;

    int specificity() const {
        int spec = 0;
        if (!id.empty()) spec += 100;
        spec += static_cast<int>(classes.size()) * 10;
        spec += static_cast<int>(pseudo_classes.size()) * 10;
        if (!tag.empty() && tag != "*") spec += 1;
        return spec;
    }
};

struct Selector {
    enum class Combinator {
        Descendant,
        Child,
    };

    std::vector<SelectorPart> parts;
    std::vector<Combinator> combinators;

    int specificity() const {
        int spec = 0;
        for (const auto& part : parts) {
            spec += part.specificity();
        }
        return spec;
    }
};

struct Declaration {
    Property property = Property::Unknown;
    std::string custom_property;
    Value value;
};

struct Rule {
    std::vector<Selector> selectors;
    std::vector<Declaration> declarations;
};

struct Stylesheet {
    std::vector<Rule> rules;
    std::unordered_set<std::string> unknown_properties;
};

}  // namespace Hummingbird::Css
