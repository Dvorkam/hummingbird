#pragma once

namespace Hummingbird {

enum class SecurityState {
    Unknown,
    Secure,
    InsecureHttp,
    InsecureTls,
};

}  // namespace Hummingbird
