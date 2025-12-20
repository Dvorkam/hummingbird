#pragma once

#include <filesystem>
#include <string_view>

namespace Hummingbird {

// Resolve an asset path starting from the current working directory and
// walking up the directory tree. Returns the first existing path found or the
// original relative path if none are found.
std::filesystem::path resolve_asset_path(std::string_view relative_path);

} // namespace Hummingbird
