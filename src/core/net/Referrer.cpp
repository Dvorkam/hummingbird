#include "core/net/Referrer.h"

#include "core/net/Origin.h"
#include "core/utils/Url.h"

namespace Hummingbird::Core {

std::optional<std::string> compute_referrer_header(std::string_view source_url, std::string_view target_url) {
    // A user-initiated navigation has no initiating document, so no referrer.
    if (source_url.empty()) {
        return std::nullopt;
    }

    auto source_origin = Origin::parse(source_url);
    auto target_origin = Origin::parse(target_url);
    // A source with no tuple origin cannot be a referrer; a non-web target is
    // not something we fetch, but guard anyway rather than leak the source.
    if (!source_origin || !target_origin) {
        return std::nullopt;
    }

    // Never reveal a secure page's URL to an insecure destination.
    if (source_origin->scheme() == "https" && target_origin->scheme() != "https") {
        return std::nullopt;
    }

    if (*source_origin == *target_origin) {
        // Same origin: the full URL, minus the fragment (never sent in Referer).
        return std::string(url_without_fragment(source_url));
    }

    // Cross origin: the origin only. serialize() already omits a default port.
    return source_origin->serialize() + "/";
}

}  // namespace Hummingbird::Core
