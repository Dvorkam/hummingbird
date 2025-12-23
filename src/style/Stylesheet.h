#pragma once

#include <string>
#include <vector>

#include "core/IGraphicsContext.h"

namespace Hummingbird::Css {

enum class SelectorType { Tag, Class, Id };

enum class Property {
    Unknown,
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
    FontSize,
    LineHeight,
    MaxWidth,
};

enum class Unit {
    Px,
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

struct Selector {
    SelectorType type;
    std::string value;

    int specificity() const {
        switch (type) {
            case SelectorType::Id:
                return 100;
            case SelectorType::Class:
                return 10;
            case SelectorType::Tag:
                return 1;
        }
        return 0;
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
