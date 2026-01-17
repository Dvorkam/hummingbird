#pragma once

#include <stddef.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

#include "core/platform_api/IImageDecoder.h"

namespace Hummingbird::Engine {

enum class ResourceType : uint8_t {
    Document,
    Stylesheet,
    Image,
};

enum class ResourceState : uint8_t {
    Requested,
    Loading,
    Ready,
    Failed,
};

struct ResourceEntry {
    ResourceType type;
    std::string url;
    ResourceState state;
    std::string body;
    std::unique_ptr<ImageBitmap> image;
};

struct ResourceView {
    ResourceType type;
    ResourceState state;
    std::string_view url;
    std::string_view body;
    const ImageBitmap* image;
};

class ResourceStore {
public:
    bool mark_ready(std::string_view url, ResourceType type, std::string body);
    bool mark_failed(std::string_view url, ResourceType type);
    bool begin_request(std::string_view url, ResourceType type);
    bool set_image(std::string_view url, ResourceType type, ImageBitmap image);

    const ResourceEntry* find(std::string_view url, ResourceType type) const;
    std::optional<ResourceView> view(std::string_view url, ResourceType type) const;

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

    std::unordered_map<ResourceKey, ResourceEntry, ResourceKeyHash, ResourceKeyEqual> resources_;
};

}  // namespace Hummingbird::Engine
