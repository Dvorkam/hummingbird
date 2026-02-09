#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

namespace Hummingbird {

enum class PixelFormat {
    BGRA32,
    PRGB32,
};

struct ImageBitmap {
    int width = 0;
    int height = 0;
    int stride = 0;
    PixelFormat format = PixelFormat::BGRA32;
    std::vector<std::uint8_t> pixels;
};

struct AnimatedImage {
    std::vector<ImageBitmap> frames;
    std::vector<int> delays_ms;
};

class IImageDecoder {
public:
    virtual ~IImageDecoder() = default;

    // Decode image bytes into a CPU-side bitmap (platform format).
    virtual std::optional<ImageBitmap> decode(std::string_view bytes) = 0;
    virtual std::optional<AnimatedImage> decode_animation(std::string_view /*bytes*/) { return std::nullopt; }
};

using ImageDecoderPtr = std::unique_ptr<IImageDecoder>;

}  // namespace Hummingbird
