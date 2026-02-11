#pragma once

#include <optional>
#include <string_view>

#include "engine/forms/FormSubmission.h"

namespace Hummingbird::DOM {
class Element;
class Node;
}

namespace Hummingbird::Engine {

std::optional<FormSubmission> build_form_submission_from_dom(const DOM::Node* dom_tree, const DOM::Element& input,
                                                             std::string_view base_url);

}  // namespace Hummingbird::Engine
