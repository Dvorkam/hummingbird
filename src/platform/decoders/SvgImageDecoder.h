#pragma once

#include <optional>
#include <string_view>

#include "core/platform_api/IImageDecoder.h"

namespace Hummingbird::Platform {

class SvgImageDecoder final : public IImageDecoder {
public:
    std::optional<ImageBitmap> decode(std::string_view bytes) override;

    // Whether these bytes are SVG at all, independent of whether we can render
    // them. The composite decoder needs the two questions kept apart: bytes
    // that ARE svg and failed to render must not then be handed to the raster
    // decoder, which cannot possibly help and whose failure message ("Couldn't
    // parse SVG image") is actively misleading about which component gave up.
    static bool sniff(std::string_view bytes);
};

}  // namespace Hummingbird::Platform
