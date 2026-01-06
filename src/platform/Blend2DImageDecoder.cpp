#include "platform/Blend2DImageDecoder.h"

#include <blend2d.h>

#include <cstdlib>
#include <cstring>

std::optional<ImageBitmap> Blend2DImageDecoder::decode(std::string_view bytes) {
    if (bytes.empty()) {
        return std::nullopt;
    }

    BLImage image;
    if (image.readFromData(bytes.data(), bytes.size()) != BL_SUCCESS) {
        return std::nullopt;
    }

    if (image.format() != BL_FORMAT_PRGB32) {
        if (image.convert(BL_FORMAT_PRGB32) != BL_SUCCESS) {
            return std::nullopt;
        }
    }

    BLImageData data{};
    if (image.getData(&data) != BL_SUCCESS) {
        return std::nullopt;
    }

    const int width = data.size.w;
    const int height = data.size.h;
    const int stride = static_cast<int>(data.stride);
    if (width <= 0 || height <= 0 || stride == 0) {
        return std::nullopt;
    }

    const size_t row_bytes = static_cast<size_t>(std::abs(stride));
    const size_t total_bytes = row_bytes * static_cast<size_t>(height);

    ImageBitmap bitmap;
    bitmap.width = width;
    bitmap.height = height;
    bitmap.stride = stride;
    bitmap.format = PixelFormat::PRGB32;
    bitmap.pixels.resize(total_bytes);

    if (stride > 0) {
        std::memcpy(bitmap.pixels.data(), data.pixelData, total_bytes);
    } else {
        const std::uint8_t* src = static_cast<const std::uint8_t*>(data.pixelData);
        for (int y = 0; y < height; ++y) {
            std::memcpy(bitmap.pixels.data() + static_cast<size_t>(y) * row_bytes,
                        src + static_cast<size_t>(height - 1 - y) * row_bytes, row_bytes);
        }
        bitmap.stride = static_cast<int>(row_bytes);
    }

    return bitmap;
}
