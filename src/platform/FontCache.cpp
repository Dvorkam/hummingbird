#include "platform/FontCache.h"

#include <bit>
#include <cstdint>
#include <ostream>
#include <utility>

#include "core/utils/Log.h"

namespace Hummingbird::Platform {

FontCache& FontCache::instance() {
    static FontCache cache;
    return cache;
}

size_t FontCache::FontKeyHash::operator()(const FontKey& key) const {
    const size_t path_hash = std::hash<std::string>{}(key.path);
    const auto size_bits = std::bit_cast<std::uint32_t>(key.size);
    const size_t size_hash = std::hash<std::uint32_t>{}(size_bits);
    return path_hash ^ (size_hash + 0x9e3779b97f4a7c15ULL + (path_hash << 6) + (path_hash >> 2));
}

const FontSetup* FontCache::get_or_load(const std::string& font_path, float font_size, bool include_error) {
    std::lock_guard<std::mutex> lock(mutex_);
    FontKey key{font_path, font_size};
    auto it = entries_.find(key);
    if (it != entries_.end()) {
        it->second.last_used = ++tick_;
        return &it->second.setup;
    }

    Entry entry;
    if (!load_font_setup(font_path, font_size, entry.setup, include_error)) {
        return nullptr;
    }
    entry.last_used = ++tick_;
    auto [inserted, ok] = entries_.emplace(std::move(key), std::move(entry));
    if (!ok) {
        return nullptr;
    }
    evict_if_needed();
    return &inserted->second.setup;
}

void FontCache::set_max_entries(size_t max_entries) {
    std::lock_guard<std::mutex> lock(mutex_);
    max_entries_ = max_entries;
    evict_if_needed();
}

bool FontCache::load_font_setup(const std::string& font_path, float font_size, FontSetup& out, bool include_error) {
    BLResult err = out.face.createFromFile(font_path.c_str());
    if (err != BL_SUCCESS) {
        if (include_error) {
            HB_LOG_ERROR("[platform] Failed to load font: " << font_path << " (err=" << err << ")");
        } else {
            HB_LOG_ERROR("[platform] Failed to load font: " << font_path);
        }
        return false;
    }

    out.font.createFromFace(out.face, font_size);
    out.metrics = out.font.metrics();
    return true;
}

void FontCache::evict_if_needed() {
    while (entries_.size() > max_entries_) {
        auto victim = entries_.end();
        for (auto it = entries_.begin(); it != entries_.end(); ++it) {
            if (victim == entries_.end() || it->second.last_used < victim->second.last_used) {
                victim = it;
            }
        }
        if (victim == entries_.end()) {
            break;
        }
        entries_.erase(victim);
    }
}

}  // namespace Hummingbird::Platform
