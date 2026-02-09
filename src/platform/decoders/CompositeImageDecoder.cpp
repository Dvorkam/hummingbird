#include "platform/decoders/CompositeImageDecoder.h"

namespace Hummingbird::Platform {

std::optional<ImageBitmap> CompositeImageDecoder::decode(std::string_view bytes) {
    if (auto svg = svg_decoder_.decode(bytes)) {
        return svg;
    }
    return raster_decoder_.decode(bytes);
}

std::optional<AnimatedImage> CompositeImageDecoder::decode_animation(std::string_view bytes) {
    return raster_decoder_.decode_animation(bytes);
}

}  // namespace Hummingbird::Platform
