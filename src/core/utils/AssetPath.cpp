#include "core/utils/AssetPath.h"

#include <cstdlib>
#include <filesystem>

namespace Hummingbird {

std::filesystem::path resolve_asset_path(std::string_view relative_path) {
    std::filesystem::path rel(relative_path);
    if (rel.is_absolute()) {
        return rel;
    }

    if (const char* asset_root = std::getenv("HB_ASSET_ROOT"); asset_root && *asset_root) {
        std::filesystem::path base(asset_root);
        auto candidate = base / rel;
        if (std::filesystem::exists(candidate)) {
            return candidate.lexically_normal();
        }
    }

    if (const char* appdir = std::getenv("APPDIR"); appdir && *appdir) {
        std::filesystem::path base(appdir);
        auto candidate = base / "usr/share/hummingbird" / rel;
        if (std::filesystem::exists(candidate)) {
            return candidate.lexically_normal();
        }
        candidate = base / rel;
        if (std::filesystem::exists(candidate)) {
            return candidate.lexically_normal();
        }
    }

    std::filesystem::path current = std::filesystem::current_path();
    for (int i = 0; i < 6; ++i) {  // Walk up to a few levels to find the repo root.
        auto candidate = current / rel;
        if (std::filesystem::exists(candidate)) {
            return candidate.lexically_normal();
        }
        if (!current.has_parent_path()) {
            break;
        }
        current = current.parent_path();
    }

    // Fallback: return the original relative path so callers can still attempt to open it.
    return rel;
}

}  // namespace Hummingbird
