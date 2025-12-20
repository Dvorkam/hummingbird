#include "core/AssetPath.h"

#include <filesystem>

namespace Hummingbird {

std::filesystem::path resolve_asset_path(std::string_view relative_path) {
    std::filesystem::path rel(relative_path);
    if (rel.is_absolute()) {
        return rel;
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
