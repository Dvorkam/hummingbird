#include "platform/script/NullScriptEngine.h"

namespace Hummingbird::Platform {

ScriptEvalResult NullScriptEngine::eval(std::string_view /*source*/, std::string_view /*filename*/) {
    return {true, {}};
}

}  // namespace Hummingbird::Platform
