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

// The fragment of |url| including the leading '#' (matching JS `location.hash`),
// or "" when there is none.
std::string_view url_fragment(std::string_view url);

// |url| with any '#fragment' removed (the "document" part, for same-document
// comparison during fragment navigation).
std::string_view url_without_fragment(std::string_view url);

// True when |url| is a `javascript:` URL (case-insensitive). Clicking one must
// not navigate — a real browser evaluates the script (and `javascript:void(0)`
// yields no navigation); we treat it as a no-op link.
bool is_javascript_url(std::string_view url);

}  // namespace Hummingbird::Core
