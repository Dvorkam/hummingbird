#include "platform/decoders/CompositeImageDecoder.h"

#include <cctype>

namespace Hummingbird::Platform {

namespace {
bool starts_with_ci(std::string_view text, std::string_view prefix) {
    if (text.size() < prefix.size()) return false;
    for (size_t i = 0; i < prefix.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(text[i])) != std::tolower(static_cast<unsigned char>(prefix[i]))) {
            return false;
        }
    }
    return true;
}

// True when the payload is an HTML document rather than an image.
//
// A server that answers an <img> request with an error page, a login wall or a
// consent interstitial sends HTML with a 200, and those pages very often carry
// an inline `<svg>` icon. SDL_image's SVG loader is permissive enough to find
// that element and render *something* — so without this guard a failed image
// silently becomes a garbage picture rather than a missing one, which is both
// wrong on screen and much harder to diagnose than a blank.
//
// Our own SVG sniffer already declines these (its doctype check requires the
// doctype to name svg); this stops the raster path from accepting what the SVG
// path deliberately refused.
bool looks_like_html_document(std::string_view bytes) {
    while (!bytes.empty() && std::isspace(static_cast<unsigned char>(bytes.front()))) {
        bytes.remove_prefix(1);
    }
    if (starts_with_ci(bytes, "<!DOCTYPE")) {
        std::string_view after = bytes.substr(std::string_view("<!DOCTYPE").size());
        while (!after.empty() && std::isspace(static_cast<unsigned char>(after.front()))) {
            after.remove_prefix(1);
        }
        return starts_with_ci(after, "html");
    }
    return starts_with_ci(bytes, "<html");
}
}  // namespace

std::optional<ImageBitmap> CompositeImageDecoder::decode(std::string_view bytes) {
    if (looks_like_html_document(bytes)) {
        return std::nullopt;
    }
    if (SvgImageDecoder::sniff(bytes)) {
        // These bytes ARE svg. Whether lunasvg rendered them or not, the raster
        // decoder cannot help — and asking it anyway is where the misleading
        // "IMG_Load_RW failed: Couldn't parse SVG image" came from, a message
        // that names SDL_image's parser for a file our own renderer had already
        // declined. The real reason is logged by SvgImageDecoder::decode.
        return svg_decoder_.decode(bytes);
    }
    return raster_decoder_.decode(bytes);
}

std::optional<AnimatedImage> CompositeImageDecoder::decode_animation(std::string_view bytes) {
    return raster_decoder_.decode_animation(bytes);
}

}  // namespace Hummingbird::Platform
