#pragma once

#include <string_view>

#include "core/ArenaAllocator.h"

namespace Hummingbird::DOM {

class Element;
class Text;

class DomFactory {
public:
    static Core::ArenaPtr<Element> create_element(Core::ArenaAllocator& arena, std::string_view tag_name);
    static Core::ArenaPtr<Text> create_text(Core::ArenaAllocator& arena, std::string_view text);
};

}  // namespace Hummingbird::DOM
