#include "platform/script/NullScriptEngine.h"

namespace Hummingbird::Platform {

void NullScriptEngine::bind_host(IScriptHost* host) {
    host_ = host;
}

ScriptEvalResult NullScriptEngine::eval(std::string_view /*source*/, std::string_view /*filename*/) {
    (void)host_;
    return {true, {}};
}

}  // namespace Hummingbird::Platform
