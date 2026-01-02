#include "core/dom/DomFactory.h"

#include "core/dom/Element.h"
#include "core/dom/Text.h"

namespace Hummingbird::DOM {

ArenaPtr<Element> DomFactory::create_element(ArenaAllocator& arena, std::string_view tag_name) {
    return Element::create(arena, tag_name);
}

ArenaPtr<Text> DomFactory::create_text(ArenaAllocator& arena, std::string_view text) {
    return Text::create(arena, text);
}

}  // namespace Hummingbird::DOM
