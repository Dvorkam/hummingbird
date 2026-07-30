#pragma once

#include <string>
#include <utility>

#include "core/platform_api/IScriptEngine.h"

namespace Hummingbird::Platform {

class ScriptEngineBase : public IScriptEngine {
public:
    void bind_host(IScriptHost* host) override { host_ = host; }
    void bind_extension_host(IExtensionApiHost* host, std::string_view extension_id) override {
        extension_host_ = host;
        extension_id_ = std::string(extension_id);
    }

protected:
    static ScriptEvalResult ok_result() { return {true, {}}; }
    static ScriptEvalResult error_result(std::string message) { return {false, std::move(message)}; }

    [[maybe_unused]] IScriptHost* host_ = nullptr;
    [[maybe_unused]] IExtensionApiHost* extension_host_ = nullptr;
    // Which extension this context belongs to (story 9.4.1). Sound because the
    // host creates one engine per extension, so a context's identity is fixed
    // for its lifetime and script running inside it cannot change or forge it.
    [[maybe_unused]] std::string extension_id_;
};

}  // namespace Hummingbird::Platform
