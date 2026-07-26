#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace Hummingbird::Core {

// A web origin: scheme + host + port (HTML "tuple origin"). This is the security
// boundary for DOM Storage — https://example.dev and http://example.dev are
// DIFFERENT origins and get separate stores — which is stricter than cookies,
// whose scope is host/domain and ignores scheme and port.
//
// Opaque origins (data:, about:, sandboxed frames) are not represented: they get
// their own storage that never persists and never matches anything, which the
// storage layer handles by simply refusing a store for a URL that has no tuple
// origin.
class Origin {
public:
    // Parses the origin of an absolute http/https URL. Returns nullopt for a URL
    // with no tuple origin (unparseable, or a non-web scheme), so callers get
    // opaque-origin behavior for free by checking the optional.
    static std::optional<Origin> parse(std::string_view url);

    const std::string& scheme() const { return scheme_; }
    const std::string& host() const { return host_; }
    uint16_t port() const { return port_; }

    bool operator==(const Origin& other) const {
        return scheme_ == other.scheme_ && host_ == other.host_ && port_ == other.port_;
    }
    bool operator!=(const Origin& other) const { return !(*this == other); }

    // "scheme://host:port" — the serialization the HTML spec uses for an origin,
    // suitable as a map key and (via key()) as part of a filename.
    std::string serialize() const;

    // A filesystem-safe key for this origin, for a per-origin storage file.
    // Stable across runs so a persisted store reloads to the same origin.
    std::string key() const;

private:
    std::string scheme_;
    std::string host_;
    uint16_t port_ = 0;
};

}  // namespace Hummingbird::Core
