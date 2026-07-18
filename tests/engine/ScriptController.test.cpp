#include <gtest/gtest.h>

#include <functional>
#include <memory>
#include <string_view>

#include "core/ArenaAllocator.h"
#include "core/dom/Element.h"
#include "core/dom/Node.h"
#include "core/platform_api/IScriptEngine.h"
#include "core/platform_api/IScriptHost.h"
#include "engine/script/DocumentScriptController.h"

namespace {
using Hummingbird::IExtensionApiHost;
using Hummingbird::IScriptEngine;
using Hummingbird::IScriptHost;
using Hummingbird::ScriptDomEvent;
using Hummingbird::ScriptEvalResult;
using Hummingbird::Core::ArenaAllocator;
using Hummingbird::DOM::Element;
using Hummingbird::DOM::Node;
using Hummingbird::Engine::DocumentScriptController;

// Fake engine that hands control back to the test at each entry point, so a
// test can re-enter the controller mid-eval to simulate a nested dispatch.
class FakeScriptEngine final : public IScriptEngine {
public:
    void bind_host(IScriptHost* host) override { host_ = host; }
    void bind_extension_host(IExtensionApiHost*) override {}
    ScriptEvalResult eval(std::string_view, std::string_view) override {
        if (on_eval) on_eval(host_);
        return {true, ""};
    }
    bool dispatch_dom_event(Node*, const ScriptDomEvent&) override {
        if (on_dispatch) on_dispatch(host_);
        return true;
    }

    IScriptHost* host_ = nullptr;
    std::function<void(IScriptHost*)> on_eval;
    std::function<void(IScriptHost*)> on_dispatch;
};
}  // namespace

// A dispatch nested inside another dispatch (a host callback re-entering the
// controller) must preserve the outer dispatch's mutation flag — the inner
// bind_host's reset must not wipe it (T-DISPATCH-REENTRANT-1, story 7.7.1).
TEST(DocumentScriptControllerTest, NestedDispatchPreservesOuterMutation) {
    ArenaAllocator arena(4096, 4);
    auto root = Element::create(arena, "div");
    root->set_attribute("id", "root");
    auto child = Element::create(arena, "span");
    child->set_attribute("id", "child");
    Node* child_ptr = child.get();
    root->append_child(std::move(child));

    auto engine = std::make_unique<FakeScriptEngine>();
    auto* engine_raw = engine.get();
    DocumentScriptController controller(std::move(engine));

    // The outer script mutates the DOM, then a host callback re-enters the
    // controller with a nested event dispatch (whose bind_host would, before
    // the fix, reset the mutation epoch to false).
    bool nested_ran = false;
    engine_raw->on_eval = [&](IScriptHost* host) {
        host->set_attribute(root.get(), "data-outer", "1");
        controller.dispatch_dom_event(root.get(), &arena, child_ptr, ScriptDomEvent{"custom", true, false, "", ""});
        nested_ran = true;
    };

    std::vector<DocumentScriptController::ScriptSource> scripts{{"outer();", "outer"}};
    const bool mutated = controller.run_scripts(scripts, root.get(), &arena);

    EXPECT_TRUE(nested_ran);
    EXPECT_TRUE(mutated);  // the outer mutation survives the nested dispatch's reset
}

// The nested dispatch does not itself drain the flag — the outermost dispatch
// owns the mutation epoch for the whole re-entrant chain.
TEST(DocumentScriptControllerTest, NestedDispatchDoesNotStealMutation) {
    ArenaAllocator arena(4096, 4);
    auto root = Element::create(arena, "div");
    root->set_attribute("id", "root");

    auto engine = std::make_unique<FakeScriptEngine>();
    auto* engine_raw = engine.get();
    DocumentScriptController controller(std::move(engine));

    // Only the nested dispatch mutates; the result belongs to the outer run.
    DocumentScriptController::ScriptDispatchResult nested_result;
    engine_raw->on_dispatch = [&](IScriptHost* host) { host->set_attribute(root.get(), "data-nested", "1"); };
    engine_raw->on_eval = [&](IScriptHost* host) {
        (void)host;
        nested_result = controller.dispatch_dom_event(root.get(), &arena, root.get(),
                                                      ScriptDomEvent{"custom", true, false, "", ""});
    };

    std::vector<DocumentScriptController::ScriptSource> scripts{{"outer();", "outer"}};
    const bool mutated = controller.run_scripts(scripts, root.get(), &arena);

    EXPECT_FALSE(nested_result.mutated);  // nested does not report the mutation...
    EXPECT_TRUE(mutated);                 // ...the outermost dispatch does
}
