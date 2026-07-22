#include "core/net/Origin.h"

#include "core/utils/StringUtils.h"
#include "core/utils/Url.h"

namespace Hummingbird::Core {

namespace {
// The default port for a scheme, so http://x and http://x:80 are the same
// origin. 0 means "no default" (an unknown scheme keeps whatever it was given).
uint16_t default_port(std::string_view scheme) {
    if (Utils::equals_ignore_case(scheme, "https")) return 443;
    if (Utils::equals_ignore_case(scheme, "http")) return 80;
    return 0;
}
}  // namespace

std::optional<Origin> Origin::parse(std::string_view url) {
    auto parts = parse_absolute_url(url);
    if (!parts) {
        return std::nullopt;
    }
    // Storage is defined only for http/https here; anything else is treated as an
    // opaque origin (no store) by returning nullopt.
    if (parts->scheme != "http" && parts->scheme != "https") {
        return std::nullopt;
    }
    if (parts->host.empty()) {
        return std::nullopt;
    }

    Origin origin;
    origin.scheme_ = parts->scheme;  // parse_absolute_url already lowercases it
    origin.host_ = parts->host;      // ...and the host
    origin.port_ = parts->port.value_or(default_port(parts->scheme));
    return origin;
}

std::string Origin::serialize() const {
    // Omit the port when it is the scheme default, matching how browsers
    // serialize an origin (https://example.dev, not https://example.dev:443).
    std::string out = scheme_ + "://" + host_;
    if (port_ != default_port(scheme_)) {
        out += ':' + std::to_string(port_);
    }
    return out;
}

std::string Origin::key() const {
    // "scheme_host_port" — no characters a filesystem objects to. The port is
    // always included here (unlike serialize) so the key is unambiguous and does
    // not depend on the default-port table staying constant.
    std::string out = scheme_ + "_" + host_ + "_" + std::to_string(port_);
    for (char& c : out) {
        if (c == ':' || c == '/' || c == '\\') c = '_';
    }
    return out;
}

}  // namespace Hummingbird::Core
