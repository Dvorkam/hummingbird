#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace Hummingbird::Core::Utils {

// A parsed `data:` URL (RFC 2397 / WHATWG "data: URL processor").
struct DataUrl {
    // Lower-cased, parameters stripped: "image/svg+xml". Defaults to
    // "text/plain" when the URL omits it, per spec.
    std::string mime_type;
    // The `charset` parameter if one was given, lower-cased; empty otherwise.
    std::string charset;
    // The decoded payload: base64-decoded when the URL said `;base64`,
    // percent-decoded otherwise.
    std::string data;
};

// Returns the parsed URL, or nullopt when `url` is not a `data:` URL or is
// malformed past recovery (no comma, undecodable base64).
//
// Lenient in the ways browsers are, because the input is markup written by hand
// as often as by a tool: whitespace inside the base64 payload is ignored
// (attribute values wrap across lines), missing base64 padding is accepted, and
// bytes that are not part of a percent-escape pass through untouched — an inline
// SVG carries literal spaces, quotes and angle brackets.
std::optional<DataUrl> parse_data_url(std::string_view url);

// True when `url` has the `data:` scheme, without parsing the rest. For callers
// that only need to route it away from the network.
bool is_data_url(std::string_view url);

}  // namespace Hummingbird::Core::Utils
