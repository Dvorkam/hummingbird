#include "engine/ResourceStore.h"

#include <gtest/gtest.h>

using Hummingbird::Engine::ResourceState;
using Hummingbird::Engine::ResourceStore;
using Hummingbird::Engine::ResourceType;

TEST(ResourceStoreTest, TracksStateTransitions) {
    ResourceStore store;

    auto& entry = store.request("https://example.dev", ResourceType::Document);
    EXPECT_EQ(entry.state, ResourceState::Requested);

    EXPECT_TRUE(store.mark_loading("https://example.dev", ResourceType::Document));
    auto view = store.view("https://example.dev", ResourceType::Document);
    ASSERT_TRUE(view.has_value());
    EXPECT_EQ(view->state, ResourceState::Loading);

    EXPECT_TRUE(store.mark_ready("https://example.dev", ResourceType::Document, "<html></html>"));
    view = store.view("https://example.dev", ResourceType::Document);
    ASSERT_TRUE(view.has_value());
    EXPECT_EQ(view->state, ResourceState::Ready);
    EXPECT_EQ(view->body, "<html></html>");

    EXPECT_TRUE(store.mark_failed("https://example.dev", ResourceType::Document));
    view = store.view("https://example.dev", ResourceType::Document);
    ASSERT_TRUE(view.has_value());
    EXPECT_EQ(view->state, ResourceState::Failed);
    EXPECT_TRUE(view->body.empty());
}
