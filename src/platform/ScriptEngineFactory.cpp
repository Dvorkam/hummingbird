#include "core/platform_api/ScriptEngineFactory.h"

#include <memory>

#include "platform/NullScriptEngine.h"

namespace Hummingbird {

ScriptEnginePtr create_script_engine() {
    return std::make_unique<Hummingbird::Platform::NullScriptEngine>();
}

}  // namespace Hummingbird
