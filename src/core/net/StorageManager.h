#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "core/net/Origin.h"
#include "core/net/StorageArea.h"

namespace Hummingbird::Core {

// Owns every origin's persistent localStorage for one profile (story 8.2.2).
// Heap-side and shared across tabs, exactly like the cookie jar: two tabs on the
// same origin see the same store, and it outlives any single document.
//
// Each origin persists to its own file (dir/<origin.key()>.tsv), so "clear this
// site's data" is a file delete, a corrupt origin cannot poison another, and the
// quota is enforced per file. Files load lazily on first touch, so startup does
// not read every origin ever visited.
class StorageManager {
public:
    // HB_STORAGE_DIR if set, else a per-profile directory beside the other
    // config files.
    static std::filesystem::path default_dir();

    explicit StorageManager(std::filesystem::path dir) : dir_(std::move(dir)) {}
    StorageManager() : StorageManager(default_dir()) {}

    // The localStorage area for `origin`, loaded from disk on first access.
    // Always returns a valid store (creating an empty one for a new origin); the
    // caller decides whether an origin is eligible before calling.
    StorageArea& area_for(const Origin& origin);

    // Writes every origin that has been touched this session back to disk.
    // Best-effort; logs per-origin failures. Returns the number of origins
    // written.
    size_t save_all() const;

    // Writes a single origin (called after a mutation, or all at shutdown).
    void save_origin(const Origin& origin) const;

    // Test/inspection: how many origin stores are currently resident.
    size_t resident_origins() const { return areas_.size(); }

private:
    void load_origin(const Origin& origin, StorageArea& area) const;
    std::filesystem::path path_for(const Origin& origin) const;

    std::filesystem::path dir_;
    // Keyed by Origin::key(). Never evicted within a session; T-STORAGE-EVICTION-1
    // will bound the on-disk aggregate.
    std::unordered_map<std::string, StorageArea> areas_;
};

}  // namespace Hummingbird::Core
