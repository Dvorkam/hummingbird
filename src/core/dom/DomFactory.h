#pragma once

#include <string_view>

#include "core/ArenaAllocator.h"

namespace Hummingbird::DOM {

class Element;
class Text;

class DomFactory {
public:
    static ArenaPtr<Element> create_element(ArenaAllocator& arena, std::string_view tag_name);
    static ArenaPtr<Text> create_text(ArenaAllocator& arena, std::string_view text);
};

}  // namespace Hummingbird::DOM
