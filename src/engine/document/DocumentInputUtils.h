#pragma once

#include <string>
#include <string_view>

namespace Hummingbird::DOM {
class Element;
}

namespace Hummingbird::Engine {

bool is_input_element(const DOM::Element* element);
bool is_textarea_element(const DOM::Element* element);
// <input> or <textarea>: the controls the engine paints as a native overlay.
bool is_text_control_element(const DOM::Element* element);
bool is_button_element(const DOM::Element* element);
bool is_interactive_control_element(const DOM::Element* element);
bool is_editable_input_element(const DOM::Element* element);
// A `<input type=checkbox>` control (7.2.6). Checkedness is reflected by the
// presence of the `checked` attribute (matches the 7.1.5 JS `.checked` MVP).
bool is_checkbox_input_element(const DOM::Element* element);
bool is_checkbox_checked(const DOM::Element& element);
bool is_autofocus_input_element(const DOM::Element* element);
std::string input_value(const DOM::Element& element);
std::string describe_input_target(const DOM::Element* element);
void set_input_value(DOM::Element& element, std::string_view value);

}  // namespace Hummingbird::Engine
