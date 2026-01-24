#include <gtest/gtest.h>

#include "core/platform_api/ScriptEngineFactory.h"

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
