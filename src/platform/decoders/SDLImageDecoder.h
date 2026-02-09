#pragma once

#include <string_view>

#include "core/platform_api/IImageDecoder.h"

namespace Hummingbird::Platform {

class SDLImageDecoder final : public IImageDecoder {
public:
    std::optional<ImageBitmap> decode(std::string_view bytes) override;
    std::optional<AnimatedImage> decode_animation(std::string_view bytes) override;
};

}  // namespace Hummingbird::Platform
