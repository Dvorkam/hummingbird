#include "core/dom/DomFactory.h"

#include "core/dom/Element.h"
#include "core/dom/Text.h"

namespace Hummingbird::DOM {

Core::ArenaPtr<Element> DomFactory::create_element(Core::ArenaAllocator& arena, std::string_view tag_name) {
    return Element::create(arena, tag_name);
}

Core::ArenaPtr<Text> DomFactory::create_text(Core::ArenaAllocator& arena, std::string_view text) {
    return Text::create(arena, text);
}

}  // namespace Hummingbird::DOM
