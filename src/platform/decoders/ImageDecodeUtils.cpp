#include "platform/decoders/ImageDecodeUtils.h"

#include <limits>

namespace Hummingbird::Platform {

std::optional<int> checked_stride(int width, int bytes_per_pixel) {
    if (width <= 0 || bytes_per_pixel <= 0) {
        return std::nullopt;
    }
    if (width > std::numeric_limits<int>::max() / bytes_per_pixel) {
        return std::nullopt;
    }
    return width * bytes_per_pixel;
}

std::optional<ImageBitmap> allocate_bitmap(int width, int height, int stride, PixelFormat format) {
    if (width <= 0 || height <= 0 || stride <= 0) {
        return std::nullopt;
    }
    const size_t row_bytes = static_cast<size_t>(stride);
    const size_t rows = static_cast<size_t>(height);
    if (row_bytes > std::numeric_limits<size_t>::max() / rows) {
        return std::nullopt;
    }

    ImageBitmap bitmap;
    bitmap.width = width;
    bitmap.height = height;
    bitmap.stride = stride;
    bitmap.format = format;
    bitmap.pixels.resize(row_bytes * rows);
    return bitmap;
}

}  // namespace Hummingbird::Platform
