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

const ImageBitmap* current_image(const ResourceEntry& entry) {
    if (entry.animation && !entry.animation->image.frames.empty()) {
        size_t index = entry.animation->frame_index % entry.animation->image.frames.size();
        return &entry.animation->image.frames[index];
    }
    return entry.image.get();
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
    // Blocked is in this list because it is a definitive answer, not a pending
    // one: re-requesting a filtered URL would only run the gate again and block
    // it again, once per layout pass.
    if (it->second.state == ResourceState::Loading || it->second.state == ResourceState::Ready ||
        it->second.state == ResourceState::Blocked) {
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
    it->second.animation.reset();
    return true;
}

bool ResourceStore::mark_blocked(std::string_view url, ResourceType type) {
    auto it = resources_.find(ResourceKeyView{type, url});
    if (it == resources_.end()) return false;
    it->second.state = ResourceState::Blocked;
    it->second.body.clear();
    it->second.image.reset();
    it->second.animation.reset();
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
    it->second.animation.reset();
    return true;
}

bool ResourceStore::set_animation(std::string_view url, ResourceType type, AnimatedImage image) {
    auto it = resources_.find(ResourceKeyView{type, url});
    if (it == resources_.end()) return false;
    if (image.frames.size() <= 1 || image.frames.size() != image.delays_ms.size()) {
        return false;
    }
    auto animation = std::make_unique<ResourceEntry::AnimationState>();
    animation->image = std::move(image);
    animation->frame_index = 0;
    animation->elapsed_ms = 0;
    it->second.animation = std::move(animation);
    it->second.image.reset();
    return true;
}

ResourceRef ResourceStore::ref_for(std::string_view url, ResourceType type) {
    if (url.empty()) {
        return {};
    }
    if (auto it = ref_by_key_.find(ResourceKeyView{type, url}); it != ref_by_key_.end()) {
        return ResourceRef{it->second, generation_};
    }
    ResourceKey key{type, std::string(url)};
    ref_slots_.push_back(key);
    const auto index = static_cast<std::uint32_t>(ref_slots_.size());  // 1-based; 0 is the null handle
    ref_by_key_.emplace(std::move(key), index);
    return ResourceRef{index, generation_};
}

const ImageBitmap* ResourceStore::resolve_image(ResourceRef ref) const {
    // A handle from a previous document, or the null handle: nothing to draw.
    // Deliberately not an error — a caller holding a stale ref is exactly the
    // situation this design makes survivable.
    if (!ref.valid() || ref.generation != generation_ || ref.index > ref_slots_.size()) {
        return nullptr;
    }
    const ResourceKey& key = ref_slots_[ref.index - 1];
    auto it = resources_.find(ResourceKeyView{key.type, key.url});
    if (it == resources_.end() || it->second.state != ResourceState::Ready) {
        return nullptr;
    }
    // Resolved per call, so an animation's current frame is always the right one
    // and nothing has to re-point anything when it advances.
    return current_image(it->second);
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
    return ResourceView{entry.type, entry.state, entry.url, entry.body, current_image(entry)};
}

bool ResourceStore::tick_animations(int delta_ms) {
    if (delta_ms <= 0) {
        return false;
    }
    bool changed = false;
    for (auto& [key, entry] : resources_) {
        (void)key;
        if (entry.state != ResourceState::Ready || !entry.animation) {
            continue;
        }
        auto& animation = *entry.animation;
        if (animation.image.frames.size() <= 1) {
            continue;
        }
        // Indexing `delays_ms` with a `frames`-bounded index is safe because
        // `set_animation` is the only way an animation enters the store and it
        // refuses one whose two vectors disagree in length. Noted here because
        // they are separate vectors on a port type, so the guarantee is not
        // local to this loop and reads like a gap until you go and check.
        animation.elapsed_ms += delta_ms;
        int delay = animation.image.delays_ms[animation.frame_index];
        if (delay <= 0) {
            delay = 100;
        }
        while (animation.elapsed_ms >= delay) {
            animation.elapsed_ms -= delay;
            animation.frame_index = (animation.frame_index + 1) % animation.image.frames.size();
            changed = true;
            delay = animation.image.delays_ms[animation.frame_index];
            if (delay <= 0) {
                delay = 100;
            }
        }
    }
    return changed;
}

void ResourceStore::clear() {
    resources_.clear();
    // Retire every handle in issue. A render tree or display list left over from
    // the previous document may still hold one, and without the generation bump
    // it would resolve against whatever reuses its slot.
    ref_slots_.clear();
    ref_by_key_.clear();
    ++generation_;
}

}  // namespace Hummingbird::Engine
