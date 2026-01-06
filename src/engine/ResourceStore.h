#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

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
};

struct ResourceView {
    ResourceType type;
    ResourceState state;
    std::string_view url;
    std::string_view body;
};

class ResourceStore {
public:
    ResourceEntry& request(std::string_view url, ResourceType type);
    bool mark_loading(std::string_view url, ResourceType type);
    bool mark_ready(std::string_view url, ResourceType type, std::string body);
    bool mark_failed(std::string_view url, ResourceType type);

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

    struct ResourceKeyHash {
        size_t operator()(const ResourceKey& key) const;
    };

    std::unordered_map<ResourceKey, ResourceEntry, ResourceKeyHash> resources_;
};

}  // namespace Hummingbird::Engine
