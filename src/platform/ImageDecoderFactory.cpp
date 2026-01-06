#include "core/platform_api/ImageDecoderFactory.h"

#include "platform/Blend2DImageDecoder.h"

ImageDecoderPtr create_image_decoder() {
    return std::make_unique<Blend2DImageDecoder>();
}
