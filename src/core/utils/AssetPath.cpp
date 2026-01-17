#include "core/utils/AssetPath.h"

#include <cstdlib>
#include <filesystem>
#include <map>
#include <mutex>
#include <string>

namespace Hummingbird::Core::Utils {

namespace {

struct CachedAssetPath {
    std::filesystem::path path;
    std::string string;
};

CachedAssetPath resolve_asset_path_uncached(std::string_view relative_path) {
    std::filesystem::path rel(relative_path);
    if (rel.is_absolute()) {
        return {rel, rel.string()};
    }

    if (const char* asset_root = std::getenv("HB_ASSET_ROOT"); asset_root && *asset_root) {
        std::filesystem::path base(asset_root);
        auto candidate = base / rel;
        if (std::filesystem::exists(candidate)) {
            auto normalized = candidate.lexically_normal();
            return {normalized, normalized.string()};
        }
    }

    if (const char* appdir = std::getenv("APPDIR"); appdir && *appdir) {
        std::filesystem::path base(appdir);
        auto candidate = base / "usr/share/hummingbird" / rel;
        if (std::filesystem::exists(candidate)) {
            auto normalized = candidate.lexically_normal();
            return {normalized, normalized.string()};
        }
        candidate = base / rel;
        if (std::filesystem::exists(candidate)) {
            auto normalized = candidate.lexically_normal();
            return {normalized, normalized.string()};
        }
    }

    std::filesystem::path current = std::filesystem::current_path();
    for (int i = 0; i < 6; ++i) {  // Walk up to a few levels to find the repo root.
        auto candidate = current / rel;
        if (std::filesystem::exists(candidate)) {
            auto normalized = candidate.lexically_normal();
            return {normalized, normalized.string()};
        }
        if (!current.has_parent_path()) {
            break;
        }
        current = current.parent_path();
    }

    // Fallback: return the original relative path so callers can still attempt to open it.
    return {rel, rel.string()};
}

const CachedAssetPath& resolve_asset_path_cached(std::string_view relative_path) {
    static std::mutex mutex;
    static std::map<std::string, CachedAssetPath> cache;
    std::lock_guard<std::mutex> lock(mutex);
    auto [it, inserted] = cache.try_emplace(std::string(relative_path));
    if (inserted) {
        it->second = resolve_asset_path_uncached(relative_path);
    }
    return it->second;
}

}  // namespace

std::filesystem::path resolve_asset_path(std::string_view relative_path) {
    return resolve_asset_path_cached(relative_path).path;
}

const std::string& resolve_asset_path_string(std::string_view relative_path) {
    return resolve_asset_path_cached(relative_path).string;
}

}  // namespace Hummingbird::Core::Utils
