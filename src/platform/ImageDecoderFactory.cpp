#include "core/platform_api/ImageDecoderFactory.h"

#include "platform/SDLImageDecoder.h"

namespace Hummingbird {

ImageDecoderPtr create_image_decoder() {
    return std::make_unique<Hummingbird::Platform::SDLImageDecoder>();
}

}  // namespace Hummingbird
