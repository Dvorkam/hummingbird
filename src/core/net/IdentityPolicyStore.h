#pragma once

#include <filesystem>
#include <string>
#include <unordered_set>

#include "core/net/BrowserIdentity.h"
#include "core/net/Origin.h"

namespace Hummingbird::Core {

// Which IdentityMode to present to each origin, for one profile. Default is
// Transparent; only origins the user has opted into Compatibility are stored, so
// an empty store means "honest everywhere". Shared across every tab (owned by
// TabManager) and persisted, mirroring CookieJar/StorageManager.
//
// Keyed by Origin::serialize() ("https://news.ycombinator.com"), which is unique
// per tuple origin and human-readable in the on-disk file.
class IdentityPolicyStore {
public:
    IdentityMode mode_for(const Origin& origin) const;
    void set_mode(const Origin& origin, IdentityMode mode);

    // Convenience: flip the origin between Transparent and Compatibility and
    // return the new mode (for the keyboard toggle).
    IdentityMode toggle(const Origin& origin);

    size_t compatibility_count() const { return compatibility_origins_.size(); }

    // --- persistence --------------------------------------------------------
    // HB_IDENTITY_FILE if set, else a file under the profile config dir.
    static std::filesystem::path default_path();

    // Writes every Compatibility origin, replacing the file. Best-effort.
    size_t save_to(const std::filesystem::path& path) const;
    // Replaces the store from `path`; a missing/corrupt file starts empty.
    size_t load_from(const std::filesystem::path& path);

private:
    std::unordered_set<std::string> compatibility_origins_;
};

}  // namespace Hummingbird::Core
