#pragma once

#include <string>
#include <string_view>

namespace Hummingbird::Html::Utils {

std::string decode_named_entities(std::string_view text);

}  // namespace Hummingbird::Html::Utils
