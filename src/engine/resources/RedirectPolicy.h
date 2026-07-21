#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace Hummingbird::Engine::RedirectPolicy {

// Browsers cap redirect chains rather than following them forever; 20 is the
// figure Chrome and Firefox both use.
inline constexpr int kMaxHops = 20;

// What to do with a response that might be a redirect. Kept a pure function of
// (status, Location, current URL, method) so the matrix is testable without a
// network.
struct Decision {
    // Absolute URL of the next hop, resolved against the current one.
    std::string url;
    // Whether the next hop stays a POST carrying the same body. When false the
    // request is rewritten to GET and the body dropped.
    bool keep_post = false;
};

inline bool is_redirect_status(long status) {
    return status == 301 || status == 302 || status == 303 || status == 307 || status == 308;
}

// RFC 9110 §15.4. 307/308 exist precisely to preserve the method; 303 mandates
// the rewrite to GET; 301/302 are specified as method-preserving but every
// browser rewrites POST to GET for them, and sites depend on that, so we match
// browsers rather than the letter of the spec. This is a deliberate deviation.
inline bool preserves_method(long status) { return status == 307 || status == 308; }

// Returns the next hop, or nullopt when this response is not a followable
// redirect (not a 3xx we handle, or no usable Location — in which case the
// response is delivered to the caller as-is, like a browser would).
std::optional<Decision> decide(long status, std::string_view location, std::string_view current_url, bool was_post);

}  // namespace Hummingbird::Engine::RedirectPolicy
