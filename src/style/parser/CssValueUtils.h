#pragma once

#include <string>
#include <vector>

#include "style/compute/Stylesheet.h"

namespace Hummingbird::Css {

std::string value_to_text(const Value& value);
std::string join_value_list(const std::vector<Value>& list);
std::string build_var_expression(const std::vector<Value>& list);
void merge_var_terms(std::vector<Value>& values);

}  // namespace Hummingbird::Css
