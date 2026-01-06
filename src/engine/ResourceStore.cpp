#include "engine/ResourceStore.h"

#include <functional>

namespace Hummingbird::Engine {

size_t ResourceStore::ResourceKeyHash::operator()(const ResourceKey& key) const {
    const size_t type_hash = std::hash<uint8_t>{}(static_cast<uint8_t>(key.type));
    const size_t url_hash = std::hash<std::string>{}(key.url);
    return type_hash ^ (url_hash << 1U);
}

ResourceEntry& ResourceStore::request(std::string_view url, ResourceType type) {
    ResourceKey key{type, std::string(url)};
    auto [it, inserted] = resources_.try_emplace(key, ResourceEntry{type, key.url, ResourceState::Requested, {}});
    if (inserted) {
        return it->second;
    }
    return it->second;
}

bool ResourceStore::mark_loading(std::string_view url, ResourceType type) {
    auto it = resources_.find(ResourceKey{type, std::string(url)});
    if (it == resources_.end()) return false;
    if (it->second.state == ResourceState::Loading || it->second.state == ResourceState::Ready) {
        return false;
    }
    it->second.state = ResourceState::Loading;
    return true;
}

bool ResourceStore::mark_ready(std::string_view url, ResourceType type, std::string body) {
    auto it = resources_.find(ResourceKey{type, std::string(url)});
    if (it == resources_.end()) return false;
    it->second.state = ResourceState::Ready;
    it->second.body = std::move(body);
    return true;
}

bool ResourceStore::mark_failed(std::string_view url, ResourceType type) {
    auto it = resources_.find(ResourceKey{type, std::string(url)});
    if (it == resources_.end()) return false;
    it->second.state = ResourceState::Failed;
    it->second.body.clear();
    return true;
}

bool ResourceStore::begin_request(std::string_view url, ResourceType type) {
    request(url, type);
    return mark_loading(url, type);
}

const ResourceEntry* ResourceStore::find(std::string_view url, ResourceType type) const {
    auto it = resources_.find(ResourceKey{type, std::string(url)});
    if (it == resources_.end()) return nullptr;
    return &it->second;
}

std::optional<ResourceView> ResourceStore::view(std::string_view url, ResourceType type) const {
    auto it = resources_.find(ResourceKey{type, std::string(url)});
    if (it == resources_.end()) return std::nullopt;
    const ResourceEntry& entry = it->second;
    return ResourceView{entry.type, entry.state, entry.url, entry.body};
}

void ResourceStore::clear() {
    resources_.clear();
}

}  // namespace Hummingbird::Engine
