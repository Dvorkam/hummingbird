#include "platform/decoders/SDLImageDecoder.h"

#include <SDL_error.h>
#include <SDL_image.h>
#include <SDL_pixels.h>
#include <SDL_rwops.h>
#include <SDL_surface.h>

#include <cstring>
#include <limits>
#include <optional>
#include <ostream>
#include <vector>

#include "core/utils/Log.h"
#include "platform/decoders/ImageDecodeUtils.h"

namespace Hummingbird::Platform {

namespace {
bool ensure_sdl_image_ready() {
    static bool initialized = false;
    if (initialized) {
        return true;
    }

    int flags = IMG_INIT_PNG | IMG_INIT_JPG | IMG_INIT_WEBP;
    int initted = IMG_Init(flags);
    if ((initted & flags) != flags) {
        HB_LOG_WARN("[image] SDL2_image init missing support: " << IMG_GetError());
    }
    initialized = true;
    return true;
}
}  // namespace

std::optional<ImageBitmap> SDLImageDecoder::decode(std::string_view bytes) {
    if (bytes.empty()) {
        return std::nullopt;
    }
    if (!ensure_sdl_image_ready()) {
        return std::nullopt;
    }

    if (bytes.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
        HB_LOG_WARN("[image] SDL2_image input too large");
        return std::nullopt;
    }

    SDL_RWops* rw = SDL_RWFromConstMem(bytes.data(), static_cast<int>(bytes.size()));
    if (!rw) {
        HB_LOG_WARN("[image] SDL_RWFromConstMem failed: " << SDL_GetError());
        return std::nullopt;
    }

    SDL_Surface* loaded = IMG_Load_RW(rw, 1);
    if (!loaded) {
        HB_LOG_WARN("[image] IMG_Load_RW failed: " << IMG_GetError());
        return std::nullopt;
    }

    SDL_Surface* converted = SDL_ConvertSurfaceFormat(loaded, SDL_PIXELFORMAT_BGRA32, 0);
    SDL_FreeSurface(loaded);
    if (!converted) {
        HB_LOG_WARN("[image] SDL_ConvertSurfaceFormat failed: " << SDL_GetError());
        return std::nullopt;
    }

    auto bitmap = allocate_bitmap(converted->w, converted->h, converted->pitch, PixelFormat::BGRA32);
    if (!bitmap) {
        SDL_FreeSurface(converted);
        return std::nullopt;
    }

    std::memcpy(bitmap->pixels.data(), converted->pixels, bitmap->pixels.size());
    SDL_FreeSurface(converted);

    return bitmap;
}

}  // namespace Hummingbird::Platform
