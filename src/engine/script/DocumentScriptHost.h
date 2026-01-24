#pragma once

#include <string>
#include <string_view>

#include "core/platform_api/IScriptHost.h"

namespace Hummingbird::Core {
class ArenaAllocator;
}

namespace Hummingbird::DOM {
class Element;
class Node;
}

namespace Hummingbird::Engine {

class DocumentScriptHost final : public IScriptHost {
public:
    void reset(DOM::Node* root, Core::ArenaAllocator* arena);
    void clear();
    bool consume_mutations();

    DOM::Element* get_element_by_id(std::string_view id) override;
    std::string get_text_content(const DOM::Element* element) override;
    void set_text_content(DOM::Element* element, std::string_view text) override;
    void set_attribute(DOM::Element* element, std::string_view name, std::string_view value) override;

private:
    DOM::Node* root_ = nullptr;
    Core::ArenaAllocator* arena_ = nullptr;
    bool mutated_ = false;
};

}  // namespace Hummingbird::Engine
