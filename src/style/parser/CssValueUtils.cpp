#include "style/parser/CssValueUtils.h"

#include "core/utils/ColorUtils.h"
#include "style/registry/CssValueNames.h"

namespace Hummingbird::Css {

std::string value_to_text(const Value& value) {
    if (value.type == Value::Type::Identifier) {
        return value.ident;
    }
    if (value.type == Value::Type::Color) {
        return Core::Utils::color_to_hex(value.color);
    }
    if (value.type == Value::Type::Length) {
        std::string out = std::to_string(value.length.value);
        if (value.length.unit == Unit::Px) {
            out += ValueNames::Px;
        } else if (value.length.unit == Unit::Em) {
            out += ValueNames::Em;
        } else if (value.length.unit == Unit::Percent) {
            out += "%";
        }
        return out;
    }
    if (value.type == Value::Type::Number) {
        return std::to_string(value.number);
    }
    if (value.type == Value::Type::Shadow) {
        return "";
    }
    return "";
}

std::string join_value_list(const std::vector<Value>& list) {
    std::string out;
    for (const auto& value : list) {
        // The `/` separator marker (see parse_value_list) is structural, not text.
        if (value.type == Value::Type::Identifier && value.ident == "/") {
            continue;
        }
        std::string piece = value_to_text(value);
        if (piece.empty()) {
            continue;
        }
        if (!out.empty()) {
            out.push_back(' ');
        }
        out += piece;
    }
    return out;
}

std::string build_var_expression(const std::vector<Value>& list) {
    if (list.empty()) {
        return "";
    }
    if (list[0].type != Value::Type::Identifier || list[0].ident != "var") {
        return "";
    }
    if (list.size() < 2 || list[1].type != Value::Type::Identifier) {
        return "";
    }
    std::string expr = "var(";
    expr += list[1].ident;
    if (list.size() >= 3) {
        std::string fallback = value_to_text(list[2]);
        if (!fallback.empty()) {
            expr += ", ";
            expr += fallback;
        }
    }
    expr += ")";
    return expr;
}

}  // namespace Hummingbird::Css
