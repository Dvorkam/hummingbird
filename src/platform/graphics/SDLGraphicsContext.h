#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>

#include "core/platform_api/IGraphicsContext.h"
#include "core/platform_api/IImageDecoder.h"
#include "layout/RenderObject.h"
#include "layout/geometry/Geometry.h"

// Forward declaration
struct SDL_Renderer;
struct SDL_Texture;

namespace Hummingbird::Platform {

class SDLGraphicsContext : public IGraphicsContext {
public:
    SDLGraphicsContext(SDL_Renderer* renderer);
    ~SDLGraphicsContext() override;

    void set_viewport(const Hummingbird::Layout::Rect& viewport) override;
    void clear(const Color& color) override;
    void present() override;
    void fill_rect(const Hummingbird::Layout::Rect& rect, const Color& color) override;
    void draw_image(const ImageBitmap& image, const Hummingbird::Layout::Rect& dest) override;
    TextMetrics measure_text(const std::string& text, const TextStyle& style) override;
    void draw_text(const std::string& text, float x, float y, const TextStyle& style) override;
    void draw_text_with_metrics(const std::string& text, float x, float y, const TextStyle& style,
                                const TextMetrics& metrics) override;
    void set_text_cache_owner(std::uint64_t owner_id) override;
    bool begin_document_cache(const Hummingbird::Layout::Rect& viewport) override;
    void end_document_cache() override;
    void draw_document_cache() override;

private:
    struct ImageCacheKey {
        const ImageBitmap* image = nullptr;
        int width = 0;
        int height = 0;
        PixelFormat format = PixelFormat::BGRA32;

        bool operator==(const ImageCacheKey& other) const {
            return image == other.image && width == other.width && height == other.height && format == other.format;
        }
    };

    struct ImageCacheKeyHash {
        size_t operator()(const ImageCacheKey& key) const;
    };

    struct ImageCacheEntry {
        SDL_Texture* texture = nullptr;
        size_t bytes = 0;
        size_t last_used = 0;
    };

    struct ImageCache {
        std::unordered_map<ImageCacheKey, ImageCacheEntry, ImageCacheKeyHash> entries;
        size_t bytes = 0;
        size_t tick = 0;
    };

    struct TextCacheKey {
        std::string text;
        std::string font_path;
        float font_size = 0.0f;
        bool bold = false;
        bool italic = false;
        bool monospace = false;
        Color color{0, 0, 0, 255};

        bool operator==(const TextCacheKey& other) const {
            return text == other.text && font_path == other.font_path && font_size == other.font_size &&
                   bold == other.bold && italic == other.italic && monospace == other.monospace &&
                   color.r == other.color.r && color.g == other.color.g && color.b == other.color.b &&
                   color.a == other.color.a;
        }
    };

    struct TextCacheKeyHash {
        size_t operator()(const TextCacheKey& key) const;
    };

    struct TextCacheEntry {
        SDL_Texture* texture = nullptr;
        int width = 0;
        int height = 0;
        size_t bytes = 0;
        size_t last_used = 0;
    };

    struct TextCache {
        std::unordered_map<TextCacheKey, TextCacheEntry, TextCacheKeyHash> entries;
        size_t bytes = 0;
        size_t tick = 0;
    };

    ImageCache& current_image_cache();
    bool should_cache_image(const Hummingbird::Layout::Rect& dest) const;
    void evict_image_cache(ImageCache& cache);
    void clear_image_caches();

    TextCache& current_text_cache();
    bool should_cache_text(const Hummingbird::Layout::Rect& dest) const;
    void evict_text_cache(TextCache& cache);
    void clear_text_caches();

    SDL_Renderer* m_renderer = nullptr;
    Hummingbird::Layout::Rect m_viewport{0, 0, 0, 0};
    std::unordered_map<std::uint64_t, ImageCache> image_caches_;
    std::unordered_map<std::uint64_t, TextCache> text_caches_;
    std::uint64_t text_cache_owner_ = 0;
    size_t image_cache_max_bytes_ = 64 * 1024 * 1024;
    size_t image_cache_max_entry_bytes_ = 8 * 1024 * 1024;
    float image_cache_margin_factor_ = 1.0f;
    size_t text_cache_max_bytes_ = 16 * 1024 * 1024;
    size_t text_cache_max_entry_bytes_ = 512 * 1024;
    size_t text_cache_max_text_length_ = 256;
    float text_cache_margin_factor_ = 1.0f;
    SDL_Texture* document_cache_ = nullptr;
    int document_cache_width_ = 0;
    int document_cache_height_ = 0;
};

}  // namespace Hummingbird::Platform
