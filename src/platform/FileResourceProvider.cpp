#include "platform/FileResourceProvider.h"

#include <fstream>

#include "core/utils/AssetPath.h"
#include "core/utils/Log.h"

std::optional<std::string> FileResourceProvider::load_text(std::string_view resource_id) {
    if (resource_id.empty()) {
        return std::nullopt;
    }

    auto path = Hummingbird::resolve_asset_path(resource_id);
    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file) {
        HB_LOG_WARN("[resource] missing text file: " << path.string());
        return std::nullopt;
    }

    std::string contents((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return contents;
}
