#pragma once

#include <string>

namespace Hummingbird::DOM {
class Element;
}

namespace Hummingbird::Engine {

enum class FormSubmitMethod {
    Get,
    Post,
};

struct FormSubmission {
    std::string url;
    FormSubmitMethod method = FormSubmitMethod::Get;
    std::string body;
    std::string content_type;
    // The <form> element being submitted, for dispatching the DOM `submit` event
    // (7.2.4.4). Valid until the document is torn down (consumed immediately).
    const DOM::Element* form_element = nullptr;
};

}  // namespace Hummingbird::Engine
