#include "style/parser/CssValueUtils.h"

#include "core/utils/ColorUtils.h"
#include "style/registry/CssValueNames.h"

namespace Hummingbird::Css {

namespace {
std::string build_var_expression_from(const std::vector<Value>& list, size_t start) {
    if (start >= list.size() || list[start].type != Value::Type::Identifier || list[start].ident != "var") {
        return "";
    }
    if (start + 1 >= list.size() || list[start + 1].type != Value::Type::Identifier) {
        return "";
    }

    std::string expr = "var(";
    expr += list[start + 1].ident;
    if (start + 2 < list.size()) {
        std::string fallback;
        if (list[start + 2].type == Value::Type::Identifier && list[start + 2].ident == "var") {
            fallback = build_var_expression_from(list, start + 2);
        } else {
            fallback = value_to_text(list[start + 2]);
        }
        if (!fallback.empty()) {
            expr += ", ";
            expr += fallback;
        }
    }
    expr += ")";
    return expr;
}
}  // namespace

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

void merge_var_terms(std::vector<Value>& values) {
    // The tokenizer drops parentheses, so `var(--x)` arrives as the two
    // identifiers [var][--x]. Merge such pairs into one `var(--x)` identifier
    // so shorthand splitters (border-radius corners) can carry the term.
    // Fallbacks are not representable here: `var(--x, 4px)` inside a longer
    // list is indistinguishable from `var(--x)` followed by a `4px` term.
    std::vector<Value> merged;
    merged.reserve(values.size());
    for (size_t i = 0; i < values.size(); ++i) {
        const bool var_head = values[i].type == Value::Type::Identifier && values[i].ident == "var" &&
                              i + 1 < values.size() && values[i + 1].type == Value::Type::Identifier &&
                              values[i + 1].ident.starts_with("--");
        if (var_head) {
            merged.push_back(Value::identifier("var(" + values[i + 1].ident + ")"));
            ++i;
            continue;
        }
        merged.push_back(values[i]);
    }
    values = std::move(merged);
}

std::string build_var_expression(const std::vector<Value>& list) {
    return build_var_expression_from(list, 0);
}

}  // namespace Hummingbird::Css
