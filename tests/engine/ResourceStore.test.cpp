#include "engine/resources/ResourceStore.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

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

// --- T-RESOURCE-REF-1 -------------------------------------------------------
// The store hands out handles instead of pointers into its own storage. These
// pin the properties that make the render tree and the retained display list
// safe to hold one: a handle survives everything the store does to the payload,
// and resolves to null rather than to freed memory.
namespace {
Hummingbird::ImageBitmap make_pixel(std::uint8_t red) {
    Hummingbird::ImageBitmap bitmap;
    bitmap.width = 1;
    bitmap.height = 1;
    bitmap.stride = 4;
    bitmap.pixels = {0, 0, red, 255};
    return bitmap;
}
}  // namespace

TEST(ResourceStoreTest, AHandleIsStableAndResolvesToTheCurrentPayload) {
    ResourceStore store;
    const std::string url = "https://example.dev/a.png";
    ASSERT_TRUE(store.begin_request(url, ResourceType::Image));

    // Minted before the bytes arrive, which is when layout actually needs one.
    const auto ref = store.ref_for(url, ResourceType::Image);
    EXPECT_TRUE(ref.valid());
    EXPECT_EQ(store.resolve_image(ref), nullptr) << "not decoded yet resolves to null, not to garbage";

    // The same (url, type) always interns to the same handle, so two elements
    // sharing an image share a handle.
    EXPECT_EQ(store.ref_for(url, ResourceType::Image), ref);

    ASSERT_TRUE(store.mark_ready(url, ResourceType::Image, "PNGDATA"));
    ASSERT_TRUE(store.set_image(url, ResourceType::Image, make_pixel(10)));
    const auto* first = store.resolve_image(ref);
    ASSERT_NE(first, nullptr);
    EXPECT_EQ(first->pixels[2], 10);

    // Replacing the payload frees the old bitmap. The handle is unaffected and
    // now resolves to the new one — under the old design this is the moment a
    // cached pointer became dangling.
    ASSERT_TRUE(store.set_image(url, ResourceType::Image, make_pixel(20)));
    const auto* second = store.resolve_image(ref);
    ASSERT_NE(second, nullptr);
    EXPECT_EQ(second->pixels[2], 20);
}

// The four store operations that free a payload. Each one used to leave any
// cached pointer dangling; none of them may make a handle unsafe.
TEST(ResourceStoreTest, AHandleSurvivesEveryOperationThatFreesThePayload) {
    ResourceStore store;
    const std::string url = "https://example.dev/b.png";
    ASSERT_TRUE(store.begin_request(url, ResourceType::Image));
    const auto ref = store.ref_for(url, ResourceType::Image);
    ASSERT_TRUE(store.mark_ready(url, ResourceType::Image, "PNGDATA"));
    ASSERT_TRUE(store.set_image(url, ResourceType::Image, make_pixel(30)));
    ASSERT_NE(store.resolve_image(ref), nullptr);

    // set_animation frees the still image it replaces.
    Hummingbird::AnimatedImage animation;
    animation.frames.push_back(make_pixel(40));
    animation.frames.push_back(make_pixel(50));
    animation.delays_ms = {40, 40};
    ASSERT_TRUE(store.set_animation(url, ResourceType::Image, std::move(animation)));
    const auto* frame = store.resolve_image(ref);
    ASSERT_NE(frame, nullptr) << "the handle now names the animation's current frame";
    EXPECT_EQ(frame->pixels[2], 40);

    // Advancing the animation changes what the SAME handle resolves to, with
    // nothing re-pointing it. That is the staleness fix.
    // 50ms against a 40ms delay advances exactly one frame; 100 would advance
    // two and wrap straight back to frame 0, testing nothing.
    ASSERT_TRUE(store.tick_animations(50));
    const auto* advanced = store.resolve_image(ref);
    ASSERT_NE(advanced, nullptr);
    EXPECT_EQ(advanced->pixels[2], 50) << "resolution happens per use, so the frame is current";

    // mark_failed drops both payloads.
    ASSERT_TRUE(store.mark_failed(url, ResourceType::Image));
    EXPECT_EQ(store.resolve_image(ref), nullptr) << "failed resolves to null, never to freed memory";
}

// Navigation. A handle minted for the previous document must not resolve
// against whatever reuses its slot — the silent-wrong-image failure.
TEST(ResourceStoreTest, AHandleFromAPreviousDocumentNeverResolves) {
    ResourceStore store;
    const std::string first_url = "https://example.dev/old.png";
    ASSERT_TRUE(store.begin_request(first_url, ResourceType::Image));
    const auto stale = store.ref_for(first_url, ResourceType::Image);
    ASSERT_TRUE(store.mark_ready(first_url, ResourceType::Image, "PNGDATA"));
    ASSERT_TRUE(store.set_image(first_url, ResourceType::Image, make_pixel(60)));
    ASSERT_NE(store.resolve_image(stale), nullptr);

    store.clear();  // navigation
    EXPECT_EQ(store.resolve_image(stale), nullptr);

    // The next document takes the same slot index. The stale handle must still
    // miss, which is what the generation is for.
    const std::string second_url = "https://example.dev/new.png";
    ASSERT_TRUE(store.begin_request(second_url, ResourceType::Image));
    const auto fresh = store.ref_for(second_url, ResourceType::Image);
    ASSERT_TRUE(store.mark_ready(second_url, ResourceType::Image, "PNGDATA"));
    ASSERT_TRUE(store.set_image(second_url, ResourceType::Image, make_pixel(70)));

    EXPECT_EQ(fresh.index, stale.index) << "the slot really was reused, so this is a real test";
    EXPECT_NE(fresh, stale);
    EXPECT_EQ(store.resolve_image(stale), nullptr) << "the old handle must not see the new document's image";
    ASSERT_NE(store.resolve_image(fresh), nullptr);
    EXPECT_EQ(store.resolve_image(fresh)->pixels[2], 70);
}

TEST(ResourceStoreTest, TheNullHandleAndUnknownHandlesResolveToNull) {
    ResourceStore store;
    EXPECT_FALSE(Hummingbird::ResourceRef{}.valid());
    EXPECT_EQ(store.resolve_image({}), nullptr);
    EXPECT_EQ(store.resolve_image(Hummingbird::ResourceRef{999, 1}), nullptr);
    EXPECT_FALSE(store.ref_for("", ResourceType::Image).valid()) << "an empty url has no identity";
}
