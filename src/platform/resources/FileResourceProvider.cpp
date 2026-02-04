#include "platform/resources/FileResourceProvider.h"

#include <optional>

#include "core/utils/AssetLoader.h"

namespace Hummingbird::Platform {

std::optional<std::string> FileResourceProvider::load_text(std::string_view resource_id) {
    return Hummingbird::Core::Utils::load_asset_text(resource_id);
}

std::optional<std::string> FileResourceProvider::load_bytes(std::string_view resource_id) {
    return Hummingbird::Core::Utils::load_asset_bytes(resource_id);
}

}  // namespace Hummingbird::Platform
