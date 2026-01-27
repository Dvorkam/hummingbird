#pragma once

#include <optional>
#include <string_view>

#include "core/platform_api/IImageDecoder.h"
#include "platform/SDLImageDecoder.h"
#include "platform/SvgImageDecoder.h"

namespace Hummingbird::Platform {

class CompositeImageDecoder final : public IImageDecoder {
public:
    std::optional<ImageBitmap> decode(std::string_view bytes) override;

private:
    SvgImageDecoder svg_decoder_;
    SDLImageDecoder raster_decoder_;
};

}  // namespace Hummingbird::Platform
