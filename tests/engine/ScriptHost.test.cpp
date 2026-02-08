#include <gtest/gtest.h>

#include "core/ArenaAllocator.h"
#include "core/dom/Element.h"
#include "core/dom/Text.h"
#include "engine/script/DocumentScriptHost.h"

TEST(DocumentScriptHostTest, FindsElementByIdAndUpdatesText) {
    Hummingbird::Core::ArenaAllocator arena(1024, 1);

    auto root = Hummingbird::DOM::Element::create(arena, "div");
    auto child = Hummingbird::DOM::Element::create(arena, "span");
    child->set_attribute("id", "target");
    child->append_child(Hummingbird::DOM::Text::create(arena, "Hello"));
    root->append_child(std::move(child));

    Hummingbird::Engine::DocumentScriptHost host;
    host.reset(root.get(), &arena);

    auto* found = host.get_element_by_id("target");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(host.get_text_content(found), "Hello");

    host.set_text_content(found, "Updated");
    EXPECT_EQ(host.get_text_content(found), "Updated");
    EXPECT_TRUE(host.consume_mutations());
    EXPECT_FALSE(host.consume_mutations());
}
