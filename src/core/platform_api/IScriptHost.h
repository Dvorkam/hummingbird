#pragma once

#include <string>
#include <string_view>

namespace Hummingbird::DOM {
class Element;
}

namespace Hummingbird {

class IScriptHost {
public:
    virtual ~IScriptHost() = default;

    virtual DOM::Element* get_element_by_id(std::string_view id) = 0;
    virtual std::string get_text_content(const DOM::Element* element) = 0;
    virtual void set_text_content(DOM::Element* element, std::string_view text) = 0;
    virtual void set_attribute(DOM::Element* element, std::string_view name, std::string_view value) = 0;
};

}  // namespace Hummingbird
