#pragma once

#include <string>

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
};

}  // namespace Hummingbird::Engine
