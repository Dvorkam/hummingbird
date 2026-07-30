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

TEST(ResourceStoreTest, AdvancesAnimatedImageFrames) {
    ResourceStore store;
    const std::string url = "https://example.dev/anim.gif";
    ASSERT_TRUE(store.begin_request(url, ResourceType::Image));
    ASSERT_TRUE(store.mark_ready(url, ResourceType::Image, "GIFDATA"));

    Hummingbird::AnimatedImage animation;
    Hummingbird::ImageBitmap first{};
    first.width = 1;
    first.height = 1;
    first.stride = 4;
    first.format = Hummingbird::PixelFormat::BGRA32;
    first.pixels = {0, 0, 0, 255};
    Hummingbird::ImageBitmap second = first;
    second.pixels = {255, 0, 0, 255};
    animation.frames.push_back(first);
    animation.frames.push_back(second);
    animation.delays_ms = {100, 100};

    ASSERT_TRUE(store.set_animation(url, ResourceType::Image, std::move(animation)));
    auto before = store.view(url, ResourceType::Image);
    ASSERT_TRUE(before.has_value());
    ASSERT_NE(before->image, nullptr);
    const Hummingbird::ImageBitmap* first_ptr = before->image;
    EXPECT_EQ(first_ptr->pixels[0], 0);

    EXPECT_FALSE(store.tick_animations(50));
    auto still_first = store.view(url, ResourceType::Image);
    ASSERT_TRUE(still_first.has_value());
    EXPECT_EQ(still_first->image, first_ptr);

    EXPECT_TRUE(store.tick_animations(60));
    auto after = store.view(url, ResourceType::Image);
    ASSERT_TRUE(after.has_value());
    ASSERT_NE(after->image, nullptr);
    EXPECT_NE(after->image, first_ptr);
    EXPECT_EQ(after->image->pixels[0], 255);
}

// The guard that makes `tick_animations` safe to index `delays_ms` with a
// `frames`-bounded index: an animation whose two vectors disagree never enters
// the store at all. They are separate vectors on a port type, so this is the
// only thing holding the invariant.
TEST(ResourceStoreTest, RefusesAnAnimationWhoseFramesAndDelaysDisagree) {
    ResourceStore store;
    const std::string url = "https://example.dev/broken.gif";
    ASSERT_TRUE(store.begin_request(url, ResourceType::Image));
    ASSERT_TRUE(store.mark_ready(url, ResourceType::Image, "GIFDATA"));

    const auto make_frames = [](int count) {
        std::vector<Hummingbird::ImageBitmap> frames;
        for (int i = 0; i < count; ++i) {
            Hummingbird::ImageBitmap frame;
            frame.width = 1;
            frame.height = 1;
            frame.stride = 4;
            frame.pixels = {0, 0, 0, 255};
            frames.push_back(std::move(frame));
        }
        return frames;
    };

    Hummingbird::AnimatedImage fewer_delays;
    fewer_delays.frames = make_frames(4);
    fewer_delays.delays_ms = {50};
    EXPECT_FALSE(store.set_animation(url, ResourceType::Image, std::move(fewer_delays)));

    Hummingbird::AnimatedImage no_delays;
    no_delays.frames = make_frames(3);
    EXPECT_FALSE(store.set_animation(url, ResourceType::Image, std::move(no_delays)));

    // A well-formed one is accepted, so the guard is rejecting the mismatch
    // rather than everything.
    Hummingbird::AnimatedImage good;
    good.frames = make_frames(3);
    good.delays_ms = {40, 40, 40};
    EXPECT_TRUE(store.set_animation(url, ResourceType::Image, std::move(good)));
    EXPECT_TRUE(store.tick_animations(500));
}
