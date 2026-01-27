#include "platform/CompositeImageDecoder.h"

namespace Hummingbird::Platform {

std::optional<ImageBitmap> CompositeImageDecoder::decode(std::string_view bytes) {
    if (auto svg = svg_decoder_.decode(bytes)) {
        return svg;
    }
    return raster_decoder_.decode(bytes);
}

}  // namespace Hummingbird::Platform
