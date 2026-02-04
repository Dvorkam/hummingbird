#pragma once

#include <optional>

#include "core/platform_api/IImageDecoder.h"

namespace Hummingbird::Platform {

std::optional<int> checked_stride(int width, int bytes_per_pixel);
std::optional<ImageBitmap> allocate_bitmap(int width, int height, int stride, PixelFormat format);

}  // namespace Hummingbird::Platform
