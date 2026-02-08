#include "core/platform_api/ScriptEngineFactory.h"

#include <memory>

#include "platform/script/QuickJSScriptEngine.h"

namespace Hummingbird {

ScriptEnginePtr create_script_engine() {
    return std::make_unique<Hummingbird::Platform::QuickJSScriptEngine>();
}

}  // namespace Hummingbird
