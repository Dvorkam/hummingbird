#include <gtest/gtest.h>

#include "core/ArenaAllocator.h"
#include "core/dom/Element.h"
#include "core/dom/Text.h"
#include "core/platform_api/ScriptEngineFactory.h"
#include "engine/script/DocumentScriptHost.h"

TEST(ScriptEngineTest, EvalSucceedsInQuickJsEngine) {
    auto engine = Hummingbird::create_script_engine();
    ASSERT_NE(engine, nullptr);

    auto result = engine->eval("1 + 1", "inline");
    EXPECT_TRUE(result.ok);
    EXPECT_TRUE(result.error.empty());
}

TEST(ScriptEngineTest, EvalReportsErrors) {
    auto engine = Hummingbird::create_script_engine();
    ASSERT_NE(engine, nullptr);

    auto result = engine->eval("function {", "inline");
    EXPECT_FALSE(result.ok);
    EXPECT_FALSE(result.error.empty());
}

TEST(ScriptEngineTest, EvalMutatesDomThroughBindings) {
    Hummingbird::Core::ArenaAllocator arena(1024, 1);
    auto root = Hummingbird::DOM::Element::create(arena, "div");
    auto target = Hummingbird::DOM::Element::create(arena, "span");
    target->set_attribute("id", "target");
    target->append_child(Hummingbird::DOM::Text::create(arena, "Hello"));
    root->append_child(std::move(target));

    Hummingbird::Engine::DocumentScriptHost host;
    host.reset(root.get(), &arena);

    auto engine = Hummingbird::create_script_engine();
    ASSERT_NE(engine, nullptr);
    engine->bind_host(&host);

    auto result = engine->eval(
        "const target = document.getElementById('target');"
        "if (target) {"
        "  target.textContent = 'Updated';"
        "  target.setAttribute('data-js', 'ok');"
        "}",
        "inline");
    EXPECT_TRUE(result.ok);
    EXPECT_TRUE(result.error.empty());

    auto* found = host.get_element_by_id("target");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(host.get_text_content(found), "Updated");
    const auto* attr = found->find_attribute("data-js");
    ASSERT_NE(attr, nullptr);
    EXPECT_EQ(*attr, "ok");
    EXPECT_TRUE(host.consume_mutations());
    EXPECT_FALSE(host.consume_mutations());
}
