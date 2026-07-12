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

std::optional<ImageBitmap> decode_surface_to_bitmap(SDL_Surface* surface) {
    if (!surface) {
        return std::nullopt;
    }
    SDL_Surface* converted = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_BGRA32, 0);
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
    auto bitmap = decode_surface_to_bitmap(loaded);
    SDL_FreeSurface(loaded);
    return bitmap;
}

std::optional<AnimatedImage> SDLImageDecoder::decode_animation(std::string_view bytes) {
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

    IMG_Animation* anim = IMG_LoadAnimation_RW(rw, 1);
    if (!anim) {
        return std::nullopt;
    }

    AnimatedImage out{};
    out.frames.reserve(static_cast<size_t>(anim->count));
    out.delays_ms.reserve(static_cast<size_t>(anim->count));
    for (int i = 0; i < anim->count; ++i) {
        auto decoded = decode_surface_to_bitmap(anim->frames[i]);
        if (!decoded) {
            IMG_FreeAnimation(anim);
            return std::nullopt;
        }
        out.frames.push_back(std::move(*decoded));
        int delay = anim->delays ? anim->delays[i] : 100;
        if (delay <= 0) {
            delay = 100;
        }
        out.delays_ms.push_back(delay);
    }
    IMG_FreeAnimation(anim);
    if (out.frames.size() <= 1) {
        return std::nullopt;
    }
    return out;
}

}  // namespace Hummingbird::Platform
