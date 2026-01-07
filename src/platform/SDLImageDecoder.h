#pragma once

#include "core/platform_api/IImageDecoder.h"

class SDLImageDecoder final : public IImageDecoder {
public:
    std::optional<ImageBitmap> decode(std::string_view bytes) override;
};
