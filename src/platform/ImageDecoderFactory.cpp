#include "core/platform_api/ImageDecoderFactory.h"

#include "platform/SDLImageDecoder.h"

ImageDecoderPtr create_image_decoder() {
    return std::make_unique<SDLImageDecoder>();
}
