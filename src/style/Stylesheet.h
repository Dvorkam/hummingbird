#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "core/platform_api/IGraphicsContext.h"

namespace Hummingbird::Css {

enum class Property {
    Unknown,
    Background,
    Border,
    Display,
    BorderWidth,
    BorderColor,
    BorderStyle,
    Margin,
    MarginTop,
    MarginRight,
    MarginBottom,
    MarginLeft,
    Padding,
    PaddingTop,
    PaddingRight,
    PaddingBottom,
    PaddingLeft,
    Width,
    Height,
    Color,
    BackgroundColor,
    FontSize,
    LineHeight,
    MaxWidth,
    TextAlign,
    TextDecoration,
    WhiteSpace,
    FontFamily,
    FontWeight,
    FontStyle,
    Float,
};

enum class Unit {
    Px,
    Em,
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
        Number,
    };

    Type type = Type::Identifier;
    std::string ident;
    Length length;
    Color color{0, 0, 0, 255};
    float number = 0.0f;

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

    static Value number_value(float value) {
        Value v;
        v.type = Type::Number;
        v.number = value;
        return v;
    }
};

struct SelectorPart {
    std::string tag;
    std::string id;
    std::vector<std::string> classes;

    int specificity() const {
        int spec = 0;
        if (!id.empty()) spec += 100;
        spec += static_cast<int>(classes.size()) * 10;
        if (!tag.empty() && tag != "*") spec += 1;
        return spec;
    }
};

struct Selector {
    std::vector<SelectorPart> parts;

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
    Value value;
};

struct Rule {
    std::vector<Selector> selectors;
    std::vector<Declaration> declarations;
};

struct Stylesheet {
    std::vector<Rule> rules;
};

}  // namespace Hummingbird::Css
