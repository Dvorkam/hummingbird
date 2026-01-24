#include <gtest/gtest.h>

#include "core/platform_api/ScriptEngineFactory.h"

TEST(ScriptEngineTest, EvalSucceedsInStubEngine) {
    auto engine = Hummingbird::create_script_engine();
    ASSERT_NE(engine, nullptr);

    auto result = engine->eval("1 + 1", "inline");
    EXPECT_TRUE(result.ok);
    EXPECT_TRUE(result.error.empty());
}
