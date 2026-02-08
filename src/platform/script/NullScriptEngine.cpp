#include "platform/script/NullScriptEngine.h"

namespace Hummingbird::Platform {

ScriptEvalResult NullScriptEngine::eval(std::string_view /*source*/, std::string_view /*filename*/) {
    (void)host_;
    return ok_result();
}

}  // namespace Hummingbird::Platform
