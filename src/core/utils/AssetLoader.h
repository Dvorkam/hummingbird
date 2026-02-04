#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace Hummingbird::Core::Utils {

std::optional<std::string> load_asset_text(std::string_view resource_id, bool log_missing = true);
std::optional<std::string> load_asset_bytes(std::string_view resource_id, bool log_missing = true);

}  // namespace Hummingbird::Core::Utils
