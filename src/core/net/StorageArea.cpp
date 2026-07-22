#include "core/net/StorageArea.h"

#include <algorithm>

namespace Hummingbird::Core {

std::vector<StorageArea::Entry>::iterator StorageArea::find(std::string_view key) {
    return std::find_if(entries_.begin(), entries_.end(), [&](const Entry& e) { return e.key == key; });
}

std::vector<StorageArea::Entry>::const_iterator StorageArea::find(std::string_view key) const {
    return std::find_if(entries_.begin(), entries_.end(), [&](const Entry& e) { return e.key == key; });
}

std::optional<std::string> StorageArea::get_item(std::string_view key) const {
    auto it = find(key);
    if (it == entries_.end()) {
        return std::nullopt;
    }
    return it->value;
}

bool StorageArea::set_item(std::string_view key, std::string_view value) {
    auto it = find(key);
    const size_t old_size = it == entries_.end() ? 0 : entry_size(it->key, it->value);
    const size_t new_size = entry_size(key, value);

    // Measure the NET change so overwriting with a smaller value never trips the
    // quota, and check before mutating so a rejected write leaves the store
    // exactly as it was.
    const size_t projected = used_bytes_ - old_size + new_size;
    if (projected > quota_bytes_) {
        return false;
    }

    if (it == entries_.end()) {
        entries_.push_back({std::string(key), std::string(value)});
    } else {
        it->value.assign(value);  // key and its insertion position are unchanged
    }
    used_bytes_ = projected;
    return true;
}

void StorageArea::remove_item(std::string_view key) {
    auto it = find(key);
    if (it == entries_.end()) {
        return;
    }
    used_bytes_ -= entry_size(it->key, it->value);
    entries_.erase(it);
}

void StorageArea::clear() {
    entries_.clear();
    used_bytes_ = 0;
}

std::optional<std::string> StorageArea::key_at(size_t index) const {
    if (index >= entries_.size()) {
        return std::nullopt;
    }
    return entries_[index].key;
}

bool StorageArea::load_entry(std::string_view key, std::string_view value) {
    if (find(key) != entries_.end()) {
        return false;
    }
    entries_.push_back({std::string(key), std::string(value)});
    used_bytes_ += entry_size(key, value);
    return true;
}

}  // namespace Hummingbird::Core
