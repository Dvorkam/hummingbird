#pragma once

#include <memory>

#include "core/platform_api/IScriptEngine.h"
#include "layout/geometry/Geometry.h"

namespace Hummingbird::Engine {

class DocumentModel;
class DocumentScriptController;

class DocumentScripting {
public:
    struct DispatchResult {
        bool handled = false;
        bool mutated = false;
    };

    explicit DocumentScripting(ScriptEnginePtr script_engine);
    ~DocumentScripting();

    DocumentScripting(const DocumentScripting&) = delete;
    DocumentScripting& operator=(const DocumentScripting&) = delete;
    DocumentScripting(DocumentScripting&&) = delete;
    DocumentScripting& operator=(DocumentScripting&&) = delete;

    void reset();
    bool run_inline_scripts(DocumentModel& model);
    DispatchResult dispatch_click(DocumentModel& model, const Layout::Rect& viewport, const Layout::Point& point,
                                  float scroll_y);
    DispatchResult dispatch_load(DocumentModel& model);

private:
    std::unique_ptr<DocumentScriptController> controller_;
};

}  // namespace Hummingbird::Engine
