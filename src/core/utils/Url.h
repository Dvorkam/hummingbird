#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace Hummingbird::Core {

struct UrlParts {
    std::string scheme;
    std::string host;
    std::optional<uint16_t> port;
    std::string path;
};

std::optional<UrlParts> parse_absolute_url(std::string_view url);

std::string normalize_input_url(std::string_view input);

// Resolve |href| relative to |base_url|. If |href| is absolute, returns it unchanged.
std::string resolve_url(std::string_view base_url, std::string_view href);

}  // namespace Hummingbird::Core
