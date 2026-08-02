#pragma once

#include <stddef.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "core/ResourceRef.h"
#include "core/platform_api/IImageDecoder.h"

namespace Hummingbird::Engine {

enum class ResourceType : uint8_t {
    Document,
    Stylesheet,
    Image,
    Font,
    Script,
    Count,  // sentinel — keep last; sizes the per-type descriptor table
};

inline constexpr size_t kResourceTypeCount = static_cast<size_t>(ResourceType::Count);

enum class ResourceState : uint8_t {
    Requested,
    Loading,
    Ready,
    // The request was attempted and did not produce a usable resource.
    Failed,
    // A declarative filter rule refused the request before it was sent (story
    // 9.4.1). Deliberately NOT Failed: nothing went wrong, and code that treats
    // "not Ready" as "something broke" would turn a blocked tracker into a
    // warning, an error page, or an endless re-request. Like Failed it is
    // terminal — there is a definitive answer for this URL.
    Blocked,
};

struct ResourceEntry {
    struct AnimationState {
        AnimatedImage image;
        size_t frame_index = 0;
        int elapsed_ms = 0;
    };

    ResourceType type;
    std::string url;
    ResourceState state;
    std::string body;
    std::unique_ptr<ImageBitmap> image;
    std::unique_ptr<AnimationState> animation;
};

struct ResourceView {
    ResourceType type;
    ResourceState state;
    std::string_view url;
    std::string_view body;
    const ImageBitmap* image;
};

// Owns every fetched resource, and hands out REFERENCES to them rather than
// pointers into its own storage (T-RESOURCE-REF-1). It is the resolver for those
// references, which is what lets `layout/` and `renderer/` name a resource they
// are forbidden to look up themselves.
class ResourceStore : public IResourceResolver {
public:
    bool mark_ready(std::string_view url, ResourceType type, std::string body);
    bool mark_failed(std::string_view url, ResourceType type);
    // Records that a filter rule refused this request (story 9.4.1). Drops any
    // payload, exactly like mark_failed — a blocked resource has nothing to show
    // — but leaves the entry distinguishable from one that was tried and broke.
    bool mark_blocked(std::string_view url, ResourceType type);
    bool begin_request(std::string_view url, ResourceType type);
    bool set_image(std::string_view url, ResourceType type, ImageBitmap image);
    bool set_animation(std::string_view url, ResourceType type, AnimatedImage image);
    bool tick_animations(int delta_ms);

    const ResourceEntry* find(std::string_view url, ResourceType type) const;
    std::optional<ResourceView> view(std::string_view url, ResourceType type) const;

    // A stable handle for (url, type), created on demand. Callers below the
    // engine hold this instead of a payload pointer; it stays valid across
    // decode, replacement and failure of the underlying resource, and resolves
    // to null rather than to freed memory once the resource is gone.
    ResourceRef ref_for(std::string_view url, ResourceType type);

    // IResourceResolver. Returns the pixels to draw for this handle right now —
    // the current frame of an animation, or the still image — or nullptr when
    // there is nothing to draw. Resolving per use is what keeps an animated
    // image current without anything re-pointing it.
    const ImageBitmap* resolve_image(ResourceRef ref) const override;

    void clear();
    size_t size() const { return resources_.size(); }

private:
    struct ResourceKey {
        ResourceType type;
        std::string url;

        bool operator==(const ResourceKey& other) const { return type == other.type && url == other.url; }
    };

    struct ResourceKeyView {
        ResourceType type;
        std::string_view url;
    };

    struct ResourceKeyHash {
        using is_transparent = void;

        size_t operator()(const ResourceKey& key) const;
        size_t operator()(const ResourceKeyView& key) const;
    };

    struct ResourceKeyEqual {
        using is_transparent = void;

        bool operator()(const ResourceKey& lhs, const ResourceKey& rhs) const {
            return lhs.type == rhs.type && lhs.url == rhs.url;
        }

        bool operator()(const ResourceKey& lhs, const ResourceKeyView& rhs) const {
            return lhs.type == rhs.type && lhs.url == rhs.url;
        }

        bool operator()(const ResourceKeyView& lhs, const ResourceKey& rhs) const {
            return lhs.type == rhs.type && lhs.url == rhs.url;
        }

        bool operator()(const ResourceKeyView& lhs, const ResourceKeyView& rhs) const {
            return lhs.type == rhs.type && lhs.url == rhs.url;
        }
    };

    ResourceEntry& request(std::string_view url, ResourceType type);
    bool mark_loading(std::string_view url, ResourceType type);

    // Interned handles. `ref_slots_` maps a 1-based ResourceRef::index to the key
    // it names; `generation_` is bumped by clear() so a handle minted for the
    // previous document cannot resolve against a slot the next one reused.
    // Resolving goes through the key rather than caching an entry pointer, so an
    // erased entry simply misses instead of dangling — which is the property
    // this whole change exists to get.
    std::vector<ResourceKey> ref_slots_;
    std::unordered_map<ResourceKey, std::uint32_t, ResourceKeyHash, ResourceKeyEqual> ref_by_key_;
    std::uint32_t generation_ = 1;

    std::unordered_map<ResourceKey, ResourceEntry, ResourceKeyHash, ResourceKeyEqual> resources_;
};

}  // namespace Hummingbird::Engine
