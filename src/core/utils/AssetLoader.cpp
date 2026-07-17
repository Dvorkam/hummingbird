#include "core/utils/AssetLoader.h"

#include <fstream>
#include <iterator>

#include "core/utils/AssetPath.h"
#include "core/utils/Log.h"

namespace Hummingbird::Core::Utils {

namespace {
std::optional<std::string> load_asset_file(std::string_view resource_id, const char* label, bool log_missing) {
    if (resource_id.empty()) {
        return std::nullopt;
    }
    // An asset id is always a repo-relative path. Refuse anything that could
    // escape it — an absolute or drive path, a UNC/SMB share ("//host", "\\host"),
    // a full URL, or a parent-dir traversal — so a page-controlled URL can never
    // reach an arbitrary or networked file. (T-SEC-URL-1: a page "//host/..." path
    // probed as a local file became a UNC/SMB request to an attacker-chosen host,
    // causing a ~21s stall on Windows.)
    if (resource_id.find("://") != std::string_view::npos) {
        return std::nullopt;
    }
    if (resource_id.front() == '/' || resource_id.front() == '\\') {
        return std::nullopt;
    }
    if (resource_id.size() >= 2 && resource_id[1] == ':') {
        return std::nullopt;
    }
    if (resource_id.find("..") != std::string_view::npos) {
        return std::nullopt;
    }

    auto path = resolve_asset_path(resource_id);
    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file) {
        if (log_missing && label) {
            HB_LOG_WARN("[resource] missing " << label << " file: " << path.string());
        }
        return std::nullopt;
    }

    std::string contents((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return contents;
}
}  // namespace

std::optional<std::string> load_asset_text(std::string_view resource_id, bool log_missing) {
    return load_asset_file(resource_id, "text", log_missing);
}

std::optional<std::string> load_asset_bytes(std::string_view resource_id, bool log_missing) {
    return load_asset_file(resource_id, "asset", log_missing);
}

}  // namespace Hummingbird::Core::Utils
