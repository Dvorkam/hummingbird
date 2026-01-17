#include "platform/FileResourceProvider.h"

#include <fstream>

#include "core/utils/AssetPath.h"
#include "core/utils/Log.h"

namespace Hummingbird::Platform {

namespace {
std::optional<std::string> load_asset_file(std::string_view resource_id, const char* label) {
    if (resource_id.empty()) {
        return std::nullopt;
    }
    if (resource_id.find("://") != std::string_view::npos) {
        return std::nullopt;
    }

    auto path = Hummingbird::Core::Utils::resolve_asset_path(resource_id);
    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file) {
        HB_LOG_WARN("[resource] missing " << label << " file: " << path.string());
        return std::nullopt;
    }

    std::string contents((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return contents;
}
}  // namespace

std::optional<std::string> FileResourceProvider::load_text(std::string_view resource_id) {
    return load_asset_file(resource_id, "text");
}

std::optional<std::string> FileResourceProvider::load_bytes(std::string_view resource_id) {
    return load_asset_file(resource_id, "asset");
}

}  // namespace Hummingbird::Platform
