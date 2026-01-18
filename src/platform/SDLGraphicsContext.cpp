#include "platform/SDLGraphicsContext.h"

#include <SDL_blendmode.h>
#include <SDL_pixels.h>
#include <SDL_rect.h>
#include <SDL_render.h>
#include <SDL_surface.h>
#include <blend2d.h>
#include <blend2d/context.h>
#include <blend2d/font.h>
#include <blend2d/fontdefs.h>
#include <blend2d/format.h>
#include <blend2d/geometry.h>
#include <blend2d/glyphbuffer.h>
#include <blend2d/image.h>
#include <blend2d/rgba.h>
#include <stddef.h>

#include <bit>
#include <cmath>
#include <cstdint>
#include <functional>
#include <ostream>
#include <span>
#include <vector>

#include "core/platform_api/IImageDecoder.h"
#include "core/utils/AssetPath.h"
#include "core/utils/Log.h"
#include "platform/Blend2DFontCache.h"

namespace Hummingbird::Platform {

namespace {
bool is_outside_viewport(const Hummingbird::Layout::Rect& viewport, float x, float y, float width, float height) {
    if (viewport.width <= 0 || viewport.height <= 0) {
        return false;
    }
    if (y + height < viewport.y || y > viewport.y + viewport.height) {
        return true;
    }
    if (x + width < viewport.x || x > viewport.x + viewport.width) {
        return true;
    }
    return false;
}

bool resolve_target_dimensions(const TextMetrics& metrics, int& target_width, int& target_height) {
    target_width = static_cast<int>(std::ceil(metrics.width));
    target_height = static_cast<int>(std::ceil(metrics.height));
    return target_width > 0 && target_height > 0;
}

SDL_Texture* build_text_texture(SDL_Renderer* renderer, const std::string& text, const TextStyle& style,
                                const FontSetup& font_setup, int target_width, int target_height) {
    BLImage img(target_width, target_height, BL_FORMAT_PRGB32);
    BLContext ctx(img);

    // Clear to transparent; text will be blended over the target.
    ctx.clearAll();

    ctx.setFillStyle(BLRgba32(style.color.r, style.color.g, style.color.b, style.color.a));
    double baseline_y = font_setup.metrics.ascent;  // place baseline inside the image
    ctx.fillUtf8Text(BLPoint(0.0, baseline_y), font_setup.font, text.c_str());
    if (style.bold) {
        ctx.fillUtf8Text(BLPoint(0.5, baseline_y), font_setup.font, text.c_str());
    }
    ctx.end();

    BLImageData imgData;
    img.getData(&imgData);
    std::span<const uint8_t> pixels{static_cast<const uint8_t*>(imgData.pixelData),
                                    static_cast<size_t>(imgData.stride) * static_cast<size_t>(target_height)};

    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormatFrom(
        const_cast<uint8_t*>(pixels.data()), target_width, target_height, 32, imgData.stride, SDL_PIXELFORMAT_BGRA32);
    if (!surface) {
        HB_LOG_ERROR("[platform] Failed to create SDL_Surface from BLImage");
        return nullptr;
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);

    if (!texture) {
        HB_LOG_ERROR("[platform] Failed to create SDL_Texture from SDL_Surface");
        return nullptr;
    }

    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    return texture;
}

float compute_text_width(const BLTextMetrics& tm) {
    float width = static_cast<float>(tm.advance.x);
    float bbox_width = static_cast<float>(tm.boundingBox.x1 - tm.boundingBox.x0);
    if (width <= 0 && bbox_width > 0) {
        width = bbox_width;
    } else if (bbox_width > width) {
        width = bbox_width;
    }
    return width;
}

float compute_text_height(const BLFontMetrics& fm) {
    return fm.ascent + fm.descent + 1.0f;  // small pad to prevent clipping
}
}  // namespace

SDLGraphicsContext::SDLGraphicsContext(SDL_Renderer* renderer) : m_renderer(renderer) {
    if (m_renderer) {
        SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
    }
}

SDLGraphicsContext::~SDLGraphicsContext() {
    clear_image_caches();
    clear_text_caches();
}

void SDLGraphicsContext::set_viewport(const Hummingbird::Layout::Rect& viewport) {
    m_viewport = viewport;
    if (!m_renderer) return;
    if (viewport.width <= 0 || viewport.height <= 0) {
        SDL_RenderSetClipRect(m_renderer, nullptr);
    } else {
        SDL_Rect clip{static_cast<int>(viewport.x), static_cast<int>(viewport.y), static_cast<int>(viewport.width),
                      static_cast<int>(viewport.height)};
        SDL_RenderSetClipRect(m_renderer, &clip);
    }
}

void SDLGraphicsContext::clear(const Color& color) {
    if (m_renderer) {
        SDL_SetRenderDrawColor(m_renderer, color.r, color.g, color.b, color.a);
        SDL_RenderClear(m_renderer);
    }
}

void SDLGraphicsContext::present() {
    if (m_renderer) {
        SDL_RenderPresent(m_renderer);
    }
}

void SDLGraphicsContext::fill_rect(const Hummingbird::Layout::Rect& rect, const Color& color) {
    if (m_renderer) {
        // Simple viewport cull
        if (m_viewport.width > 0 && m_viewport.height > 0) {
            if (rect.y + rect.height < m_viewport.y || rect.y > m_viewport.y + m_viewport.height) {
                return;
            }
            if (rect.x + rect.width < m_viewport.x || rect.x > m_viewport.x + m_viewport.width) {
                return;
            }
        }
        SDL_SetRenderDrawColor(m_renderer, color.r, color.g, color.b, color.a);
        SDL_Rect sdl_rect = {(int)rect.x, (int)rect.y, (int)rect.width, (int)rect.height};
        SDL_RenderFillRect(m_renderer, &sdl_rect);
    }
}

size_t SDLGraphicsContext::TextCacheKeyHash::operator()(const TextCacheKey& key) const {
    const size_t text_hash = std::hash<std::string>{}(key.text);
    const size_t path_hash = std::hash<std::string>{}(key.font_path);
    const auto size_bits = std::bit_cast<std::uint32_t>(key.font_size);
    size_t hash = text_hash ^ (path_hash + 0x9e3779b97f4a7c15ULL + (text_hash << 6) + (text_hash >> 2));
    hash ^= std::hash<std::uint32_t>{}(size_bits) + 0x9e3779b97f4a7c15ULL + (hash << 6) + (hash >> 2);
    hash ^= static_cast<size_t>(key.bold) << 1;
    hash ^= static_cast<size_t>(key.italic) << 2;
    hash ^= static_cast<size_t>(key.monospace) << 3;
    hash ^= static_cast<size_t>(key.color.r) << 8;
    hash ^= static_cast<size_t>(key.color.g) << 16;
    hash ^= static_cast<size_t>(key.color.b) << 24;
    hash ^= static_cast<size_t>(key.color.a) << 32;
    return hash;
}

size_t SDLGraphicsContext::ImageCacheKeyHash::operator()(const ImageCacheKey& key) const {
    size_t hash = std::hash<const ImageBitmap*>{}(key.image);
    hash ^= std::hash<int>{}(key.width) + 0x9e3779b97f4a7c15ULL + (hash << 6) + (hash >> 2);
    hash ^= std::hash<int>{}(key.height) + 0x9e3779b97f4a7c15ULL + (hash << 6) + (hash >> 2);
    hash ^= std::hash<int>{}(static_cast<int>(key.format)) + 0x9e3779b97f4a7c15ULL + (hash << 6) + (hash >> 2);
    return hash;
}

SDLGraphicsContext::ImageCache& SDLGraphicsContext::current_image_cache() {
    return image_caches_[text_cache_owner_];
}

bool SDLGraphicsContext::should_cache_image(const Hummingbird::Layout::Rect& dest) const {
    if (text_cache_owner_ == 0) {
        return false;
    }
    if (m_viewport.width <= 0.0f || m_viewport.height <= 0.0f) {
        return true;
    }
    const float margin = m_viewport.height * image_cache_margin_factor_;
    Hummingbird::Layout::Rect expanded{m_viewport.x, m_viewport.y - margin, m_viewport.width,
                                       m_viewport.height + margin * 2.0f};
    return !is_outside_viewport(expanded, dest.x, dest.y, dest.width, dest.height);
}

void SDLGraphicsContext::evict_image_cache(ImageCache& cache) {
    while (cache.bytes > image_cache_max_bytes_ && !cache.entries.empty()) {
        auto victim = cache.entries.end();
        for (auto it = cache.entries.begin(); it != cache.entries.end(); ++it) {
            if (victim == cache.entries.end() || it->second.last_used < victim->second.last_used) {
                victim = it;
            }
        }
        if (victim == cache.entries.end()) {
            break;
        }
        if (victim->second.texture) {
            SDL_DestroyTexture(victim->second.texture);
        }
        cache.bytes -= victim->second.bytes;
        HB_LOG_DEBUG("[perf] image cache evict owner=" << text_cache_owner_ << " bytes=" << victim->second.bytes
                                                       << " size=" << victim->first.width << "x"
                                                       << victim->first.height);
        cache.entries.erase(victim);
    }
}

void SDLGraphicsContext::clear_image_caches() {
    for (auto& [owner, cache] : image_caches_) {
        for (auto& [key, entry] : cache.entries) {
            if (entry.texture) {
                SDL_DestroyTexture(entry.texture);
            }
        }
    }
    image_caches_.clear();
}

SDLGraphicsContext::TextCache& SDLGraphicsContext::current_text_cache() {
    return text_caches_[text_cache_owner_];
}

bool SDLGraphicsContext::should_cache_text(const Hummingbird::Layout::Rect& dest) const {
    if (text_cache_owner_ == 0) {
        return false;
    }
    if (m_viewport.width <= 0.0f || m_viewport.height <= 0.0f) {
        return true;
    }
    const float margin = m_viewport.height * text_cache_margin_factor_;
    Hummingbird::Layout::Rect expanded{m_viewport.x, m_viewport.y - margin, m_viewport.width,
                                       m_viewport.height + margin * 2.0f};
    return !is_outside_viewport(expanded, dest.x, dest.y, dest.width, dest.height);
}

void SDLGraphicsContext::evict_text_cache(TextCache& cache) {
    while (cache.bytes > text_cache_max_bytes_ && !cache.entries.empty()) {
        auto victim = cache.entries.end();
        for (auto it = cache.entries.begin(); it != cache.entries.end(); ++it) {
            if (victim == cache.entries.end() || it->second.last_used < victim->second.last_used) {
                victim = it;
            }
        }
        if (victim == cache.entries.end()) {
            break;
        }
        if (victim->second.texture) {
            SDL_DestroyTexture(victim->second.texture);
        }
        cache.bytes -= victim->second.bytes;
        HB_LOG_DEBUG("[perf] text cache evict owner=" << text_cache_owner_ << " bytes=" << victim->second.bytes
                                                      << " text_len=" << victim->first.text.size());
        cache.entries.erase(victim);
    }
}

void SDLGraphicsContext::clear_text_caches() {
    for (auto& [owner, cache] : text_caches_) {
        for (auto& [key, entry] : cache.entries) {
            if (entry.texture) {
                SDL_DestroyTexture(entry.texture);
            }
        }
    }
    text_caches_.clear();
}

void SDLGraphicsContext::draw_image(const ImageBitmap& image, const Hummingbird::Layout::Rect& dest) {
    if (!m_renderer) {
        return;
    }
    if (image.width <= 0 || image.height <= 0 || image.pixels.empty()) {
        return;
    }
    if (dest.width <= 0.0f || dest.height <= 0.0f) {
        return;
    }
    if (is_outside_viewport(m_viewport, dest.x, dest.y, dest.width, dest.height)) {
        return;
    }

    SDL_Texture* texture = nullptr;
    bool cached = false;

    const size_t entry_bytes =
        static_cast<size_t>(image.width) * static_cast<size_t>(image.height) * sizeof(std::uint32_t);
    const bool cache_allowed =
        should_cache_image(dest) && entry_bytes <= image_cache_max_entry_bytes_ && text_cache_owner_ != 0;

    ImageCache* cache = nullptr;
    ImageCacheKey key;
    if (cache_allowed) {
        cache = &current_image_cache();
        key.image = &image;
        key.width = image.width;
        key.height = image.height;
        key.format = image.format;

        auto it = cache->entries.find(key);
        if (it != cache->entries.end()) {
            it->second.last_used = ++cache->tick;
            texture = it->second.texture;
            cached = true;
        }
    }

    if (!texture) {
        SDL_Surface* surface =
            SDL_CreateRGBSurfaceWithFormatFrom(const_cast<std::uint8_t*>(image.pixels.data()), image.width,
                                               image.height, 32, image.stride, SDL_PIXELFORMAT_BGRA32);
        if (!surface) {
            HB_LOG_ERROR("[platform] Failed to create SDL_Surface from image");
            return;
        }

        texture = SDL_CreateTextureFromSurface(m_renderer, surface);
        SDL_FreeSurface(surface);
        if (!texture) {
            HB_LOG_ERROR("[platform] Failed to create SDL_Texture from image");
            return;
        }

        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);

        if (cache_allowed && cache) {
            ImageCacheEntry entry;
            entry.texture = texture;
            entry.bytes = entry_bytes;
            entry.last_used = ++cache->tick;
            cache->bytes += entry.bytes;
            cache->entries.emplace(std::move(key), entry);
            evict_image_cache(*cache);
            cached = true;
        }
    }

    SDL_Rect dest_rect = {static_cast<int>(dest.x), static_cast<int>(dest.y), static_cast<int>(dest.width),
                          static_cast<int>(dest.height)};
    SDL_RenderCopy(m_renderer, texture, nullptr, &dest_rect);

    if (!cached) {
        SDL_DestroyTexture(texture);
    }
}

void SDLGraphicsContext::draw_text(const std::string& text, float x, float y, const TextStyle& style) {
    if (!m_renderer) {
        return;
    }

    TextMetrics metrics = measure_text(text, style);
    draw_text_with_metrics(text, x, y, style, metrics);
}

void SDLGraphicsContext::draw_text_with_metrics(const std::string& text, float x, float y, const TextStyle& style,
                                                const TextMetrics& metrics) {
    if (!m_renderer) {
        return;
    }

    int target_width = 0;
    int target_height = 0;
    if (!resolve_target_dimensions(metrics, target_width, target_height)) {
        HB_LOG_DEBUG("[draw_text] measured zero size for '" << text << "'");
        return;
    }
    if (is_outside_viewport(m_viewport, x, y, target_width, target_height)) return;

    const std::string& resolved_font = Hummingbird::Core::Utils::resolve_asset_path_string(style.font_path);
    const FontSetup* font_setup = Blend2DFontCache::instance().get_or_load(resolved_font, style.font_size, false);
    if (!font_setup) return;

    SDL_Texture* texture = nullptr;
    bool cached = false;

    const size_t entry_bytes =
        static_cast<size_t>(target_width) * static_cast<size_t>(target_height) * sizeof(std::uint32_t);
    const bool cache_allowed =
        should_cache_text({x, y, static_cast<float>(target_width), static_cast<float>(target_height)}) &&
        !text.empty() && text.size() <= text_cache_max_text_length_ && entry_bytes <= text_cache_max_entry_bytes_;

    TextCache* cache = nullptr;
    TextCacheKey key;
    if (cache_allowed) {
        cache = &current_text_cache();
        key.text = text;
        key.font_path = resolved_font;
        key.font_size = style.font_size;
        key.bold = style.bold;
        key.italic = style.italic;
        key.monospace = style.monospace;
        key.color = style.color;

        auto it = cache->entries.find(key);
        if (it != cache->entries.end()) {
            it->second.last_used = ++cache->tick;
            texture = it->second.texture;
            cached = true;
        }
    }

    if (!texture) {
        texture = build_text_texture(m_renderer, text, style, *font_setup, target_width, target_height);
        if (!texture) return;

        if (cache_allowed && cache) {
            TextCacheEntry entry;
            entry.texture = texture;
            entry.width = target_width;
            entry.height = target_height;
            entry.bytes = entry_bytes;
            entry.last_used = ++cache->tick;
            cache->bytes += entry.bytes;
            cache->entries.emplace(std::move(key), entry);
            evict_text_cache(*cache);
            cached = true;
        }
    }

    SDL_Rect dest_rect = {(int)x, (int)y, target_width, target_height};

    static bool logged = false;
    if (!logged) {
        HB_LOG_DEBUG("[draw_text] text='" << text << "' at (" << x << ", " << y << ") size=(" << target_width << ", "
                                          << target_height << ") font=" << resolved_font);
        logged = true;
    }

    SDL_RenderCopy(m_renderer, texture, NULL, &dest_rect);

    if (!cached) {
        SDL_DestroyTexture(texture);
    }
}

TextMetrics SDLGraphicsContext::measure_text(const std::string& text, const TextStyle& style) {
    if (text.empty()) {
        return {0, 0};
    }

    const std::string& resolved_font = Hummingbird::Core::Utils::resolve_asset_path_string(style.font_path);
    const FontSetup* font_setup = Blend2DFontCache::instance().get_or_load(resolved_font, style.font_size, true);
    if (!font_setup) return {0, 0};

    BLGlyphBuffer glyphBuffer;
    glyphBuffer.setUtf8Text(text.c_str());
    font_setup->font.shape(glyphBuffer);

    BLTextMetrics tm;
    font_setup->font.getTextMetrics(glyphBuffer, tm);

    // Prefer advance width but guard with bounding box to avoid clipping.
    float width = compute_text_width(tm);

    // Simple approximations for bold/italic when only a regular font is available.
    if (style.bold) width += 1.0f;
    if (style.italic) width += 1.0f;

    // Use font metrics for a consistent line height with a small fudge for descenders.
    float height = compute_text_height(font_setup->metrics);

    static bool logged = false;
    if (!logged) {
        HB_LOG_DEBUG("[measure_text] path=" << resolved_font << " text='" << text << "' size=" << style.font_size
                                            << " -> (" << width << ", " << height << ")");
        logged = true;
    }

    return {width, height};
}

void SDLGraphicsContext::set_text_cache_owner(std::uint64_t owner_id) {
    text_cache_owner_ = owner_id;
}

}  // namespace Hummingbird::Platform
