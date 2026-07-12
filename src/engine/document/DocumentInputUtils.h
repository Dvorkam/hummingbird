#pragma once

#include <string>
#include <string_view>

namespace Hummingbird::DOM {
class Element;
}

namespace Hummingbird::Engine {

bool is_input_element(const DOM::Element* element);
bool is_button_element(const DOM::Element* element);
bool is_interactive_control_element(const DOM::Element* element);
bool is_editable_input_element(const DOM::Element* element);
bool is_autofocus_input_element(const DOM::Element* element);
std::string input_value(const DOM::Element& element);
std::string describe_input_target(const DOM::Element* element);
void set_input_value(DOM::Element& element, std::string_view value);

}  // namespace Hummingbird::Engine
