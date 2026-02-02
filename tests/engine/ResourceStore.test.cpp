#include "engine/resources/ResourceStore.h"

#include <gtest/gtest.h>

using Hummingbird::Engine::ResourceState;
using Hummingbird::Engine::ResourceStore;
using Hummingbird::Engine::ResourceType;

TEST(ResourceStoreTest, TracksStateTransitions) {
    ResourceStore store;

    EXPECT_TRUE(store.begin_request("https://example.dev", ResourceType::Document));
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

    EXPECT_TRUE(store.begin_request("https://example.dev", ResourceType::Document));
    view = store.view("https://example.dev", ResourceType::Document);
    ASSERT_TRUE(view.has_value());
    EXPECT_EQ(view->state, ResourceState::Loading);
}

TEST(ResourceStoreTest, DedupesRequestsByUrlAndType) {
    ResourceStore store;

    EXPECT_TRUE(store.begin_request("https://example.dev/style.css", ResourceType::Stylesheet));
    EXPECT_FALSE(store.begin_request("https://example.dev/style.css", ResourceType::Stylesheet));

    EXPECT_TRUE(store.mark_failed("https://example.dev/style.css", ResourceType::Stylesheet));
    EXPECT_TRUE(store.begin_request("https://example.dev/style.css", ResourceType::Stylesheet));
}
