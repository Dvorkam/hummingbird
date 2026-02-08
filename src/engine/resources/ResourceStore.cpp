#include "engine/resources/ResourceStore.h"

#include <functional>
#include <utility>

namespace Hummingbird::Engine {

namespace {
size_t hash_key(ResourceType type, std::string_view url) {
    const size_t type_hash = std::hash<uint8_t>{}(static_cast<uint8_t>(type));
    const size_t url_hash = std::hash<std::string_view>{}(url);
    return type_hash ^ (url_hash << 1U);
}
}  // namespace

size_t ResourceStore::ResourceKeyHash::operator()(const ResourceKey& key) const {
    return hash_key(key.type, key.url);
}

size_t ResourceStore::ResourceKeyHash::operator()(const ResourceKeyView& key) const {
    return hash_key(key.type, key.url);
}

ResourceEntry& ResourceStore::request(std::string_view url, ResourceType type) {
    ResourceKey key{type, std::string(url)};
    auto [it, inserted] = resources_.try_emplace(key, ResourceEntry{type, key.url, ResourceState::Requested, {}, {}});
    if (inserted) {
        return it->second;
    }
    return it->second;
}

bool ResourceStore::mark_loading(std::string_view url, ResourceType type) {
    auto it = resources_.find(ResourceKeyView{type, url});
    if (it == resources_.end()) return false;
    if (it->second.state == ResourceState::Loading || it->second.state == ResourceState::Ready) {
        return false;
    }
    it->second.state = ResourceState::Loading;
    return true;
}

bool ResourceStore::mark_ready(std::string_view url, ResourceType type, std::string body) {
    auto it = resources_.find(ResourceKeyView{type, url});
    if (it == resources_.end()) return false;
    it->second.state = ResourceState::Ready;
    it->second.body = std::move(body);
    return true;
}

bool ResourceStore::mark_failed(std::string_view url, ResourceType type) {
    auto it = resources_.find(ResourceKeyView{type, url});
    if (it == resources_.end()) return false;
    it->second.state = ResourceState::Failed;
    it->second.body.clear();
    it->second.image.reset();
    return true;
}

bool ResourceStore::begin_request(std::string_view url, ResourceType type) {
    request(url, type);
    return mark_loading(url, type);
}

bool ResourceStore::set_image(std::string_view url, ResourceType type, ImageBitmap image) {
    auto it = resources_.find(ResourceKeyView{type, url});
    if (it == resources_.end()) return false;
    it->second.image = std::make_unique<ImageBitmap>(std::move(image));
    return true;
}

const ResourceEntry* ResourceStore::find(std::string_view url, ResourceType type) const {
    auto it = resources_.find(ResourceKeyView{type, url});
    if (it == resources_.end()) return nullptr;
    return &it->second;
}

std::optional<ResourceView> ResourceStore::view(std::string_view url, ResourceType type) const {
    auto it = resources_.find(ResourceKeyView{type, url});
    if (it == resources_.end()) return std::nullopt;
    const ResourceEntry& entry = it->second;
    return ResourceView{entry.type, entry.state, entry.url, entry.body, entry.image.get()};
}

void ResourceStore::clear() {
    resources_.clear();
}

}  // namespace Hummingbird::Engine
