#include "core/platform_api/ImageDecoderFactory.h"

#include <memory>

#include "platform/CompositeImageDecoder.h"

namespace Hummingbird {

ImageDecoderPtr create_image_decoder() {
    return std::make_unique<Hummingbird::Platform::CompositeImageDecoder>();
}

}  // namespace Hummingbird
