#pragma once

#include <optional>
#include <string_view>

#include "core/platform_api/IImageDecoder.h"

namespace Hummingbird::Platform {

class SvgImageDecoder final : public IImageDecoder {
public:
    std::optional<ImageBitmap> decode(std::string_view bytes) override;
};

}  // namespace Hummingbird::Platform
