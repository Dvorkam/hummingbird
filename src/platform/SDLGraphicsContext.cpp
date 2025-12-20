#include "platform/SDLGraphicsContext.h"
#include "core/AssetPath.h"
#include <SDL.h>
#include <blend2d.h>
#include <iostream>
#include <span>

SDLGraphicsContext::SDLGraphicsContext(SDL_Renderer* renderer) : m_renderer(renderer) {
    if (m_renderer) {
        SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
    }
}

SDLGraphicsContext::~SDLGraphicsContext() {
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
        SDL_SetRenderDrawColor(m_renderer, color.r, color.g, color.b, color.a);
        SDL_Rect sdl_rect = { (int)rect.x, (int)rect.y, (int)rect.width, (int)rect.height };
        SDL_RenderFillRect(m_renderer, &sdl_rect);
    }
}

void SDLGraphicsContext::draw_text(const std::string& text, float x, float y, const std::string& font_path, float font_size, const Color& color) {
    if (!m_renderer) {
        return;
    }

    // Reuse the layout metrics logic to avoid duplicating measurement behavior.
    TextMetrics metrics = measure_text(text, font_path, font_size);
    int target_width = static_cast<int>(std::ceil(metrics.width));
    int target_height = static_cast<int>(std::ceil(metrics.height));
    if (target_width <= 0 || target_height <= 0) {
        std::cerr << "[draw_text] measured zero size for '" << text << "'\n";
        return;
    }

    auto resolved_font = Hummingbird::resolve_asset_path(font_path);

    BLFontFace face;
    if (face.createFromFile(resolved_font.string().c_str()) != BL_SUCCESS) {
        std::cerr << "Failed to load font: " << resolved_font << std::endl;
        return;
    }

    BLFont font;
    font.createFromFace(face, font_size);

    BLFontMetrics fm = font.metrics();

    BLImage img(target_width, target_height, BL_FORMAT_PRGB32);
    BLContext ctx(img);

    // Clear to transparent; text will be blended over the target.
    ctx.clearAll();

    ctx.setFillStyle(BLRgba32(color.r, color.g, color.b, color.a));
    double baseline_y = fm.ascent; // place baseline inside the image
    ctx.fillUtf8Text(BLPoint(0.0, baseline_y), font, text.c_str());
    ctx.end();

    BLImageData imgData;
    img.getData(&imgData);
    std::span<const uint8_t> pixels{
        static_cast<const uint8_t*>(imgData.pixelData),
        static_cast<size_t>(imgData.stride) * static_cast<size_t>(target_height)
    };

    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormatFrom(
        const_cast<uint8_t*>(pixels.data()),
        target_width,
        target_height,
        32,
        imgData.stride,
        SDL_PIXELFORMAT_BGRA32);
    if (!surface) {
        std::cerr << "Failed to create SDL_Surface from BLImage" << std::endl;
        return;
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(m_renderer, surface);
    SDL_FreeSurface(surface);

    if (!texture) {
        std::cerr << "Failed to create SDL_Texture from SDL_Surface" << std::endl;
        return;
    }

        SDL_Rect dest_rect = { (int)x, (int)y, target_width, target_height };

        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);

        static bool logged = false;
        if (!logged) {
            std::cerr << "[draw_text] text='" << text << "' at (" << x << ", " << y << ") size=(" << target_width << ", " << target_height
                      << ") font=" << resolved_font << "\n";
            logged = true;
        }

        SDL_RenderCopy(m_renderer, texture, NULL, &dest_rect);

        SDL_DestroyTexture(texture);

    }

    

    TextMetrics SDLGraphicsContext::measure_text(const std::string& text, const std::string& font_path, float font_size) {

        if (text.empty()) {

            return { 0, 0 };

        }

    

        auto resolved_font = Hummingbird::resolve_asset_path(font_path);

        BLFontFace face;

        BLResult err = face.createFromFile(resolved_font.string().c_str());

        if (err != BL_SUCCESS) {

            std::cerr << "Failed to load font: " << resolved_font << " (err=" << err << ")" << std::endl;

            return {0, 0};

        }

    

        BLFont font;

        font.createFromFace(face, font_size);

    

        BLGlyphBuffer glyphBuffer;

        glyphBuffer.setUtf8Text(text.c_str());

        font.shape(glyphBuffer);

    

        BLTextMetrics tm;

        font.getTextMetrics(glyphBuffer, tm);

    

        // Using bounding box for width is correct for layout.

        float width = (float)(tm.boundingBox.x1 - tm.boundingBox.x0);

    

        // Use font metrics for a consistent line height.

        BLFontMetrics fm = font.metrics();

        float height = fm.ascent + fm.descent; // lineGap is often too much for web layout

    

        static bool logged = false;
        if (!logged) {
            std::cerr << "[measure_text] path=" << resolved_font << " text='" << text << "' size=" << font_size
                      << " -> (" << width << ", " << height << ")\n";
            logged = true;
        }

        return {width, height};

    }

    
