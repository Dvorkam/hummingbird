#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/platform_api/IExtensionApiHost.h"
#include "core/platform_api/IScriptHost.h"

namespace Hummingbird {

namespace DOM {
class Node;
}

struct ScriptEvalResult {
    bool ok = false;
    std::string error;
};

// A DOM event the engine (app/input side) asks the script engine to dispatch to a
// target node through the 7.2.3 propagation pipeline (7.2.4). Platform-agnostic:
// the engine fills the fields, the adapter builds the JS Event.
struct ScriptDomEvent {
    std::string type;
    bool bubbles = true;
    bool cancelable = true;
    std::string key;   // keyboard events
    std::string code;  // keyboard events
};

class IScriptEngine {
public:
    virtual ~IScriptEngine() = default;

    virtual void bind_host(IScriptHost* host) = 0;
    virtual void bind_extension_host(IExtensionApiHost* host) = 0;
    virtual ScriptEvalResult eval(std::string_view source, std::string_view filename = "inline") = 0;

    // Drops any per-document state the engine caches across evals (e.g. DOM node
    // wrappers holding raw node pointers). Called on navigation, before the
    // document's arena is reset, so no stale handle survives into the next page.
    virtual void reset_bindings() {}

    // Dispatches `event` to `target` through the DOM event pipeline (capture/
    // target/bubble). Returns false when a listener called preventDefault (the
    // caller should skip the default action). Default: no-op returning true.
    virtual bool dispatch_dom_event(DOM::Node* /*target*/, const ScriptDomEvent& /*event*/) { return true; }

    // Sets the document URL backing window.location (called on navigation/before
    // scripts run). Does not fire hashchange. Default: no-op.
    virtual void set_location(std::string_view /*url*/) {}

    // Same-document fragment navigation (7.2.5): points window.location at `url`
    // and fires `hashchange` when the fragment changed, WITHOUT reloading. Returns
    // true when a hashchange fired. Default: no-op returning false.
    virtual bool navigate_fragment(std::string_view /*url*/) { return false; }

    // Returns and clears the URL a *script* set via `location.hash = ...` since
    // the last consume, so the app can reflect it in the URL bar / tab history
    // (7.7.3). App-initiated navigation (navigate_fragment / set_location) does
    // NOT report here. Default: nothing.
    virtual std::optional<std::string> consume_location_change() { return std::nullopt; }

    // Timer scheduling (7.3.1). `run_due_timers` fires every setTimeout/setInterval
    // callback whose deadline has passed at `now_ms` — a monotonically
    // non-decreasing, document-relative clock in milliseconds — in deadline then
    // registration order; it returns true if any callback ran. `has_pending_timers`
    // reports whether any timer is still scheduled, so the driver knows to keep
    // ticking. Timers are per-document and are dropped by reset_bindings. Defaults:
    // no-op.
    virtual bool run_due_timers(double /*now_ms*/) { return false; }
    virtual bool has_pending_timers() const { return false; }

    // Missing-API telemetry (7.5.2): names of unimplemented JS APIs the current
    // page touched, deduped and in first-touch order. Fail-soft — touching such
    // an API logs once and no-ops rather than throwing, so the rest of the script
    // still runs. Reset per document. Empty by default.
    virtual std::vector<std::string> missing_apis() const { return {}; }
};

using ScriptEnginePtr = std::unique_ptr<IScriptEngine>;

}  // namespace Hummingbird
