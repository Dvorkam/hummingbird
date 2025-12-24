#include "platform/SDLGraphicsContext.h"

#include <SDL.h>
#include <blend2d.h>

#include <iostream>
#include <span>

#include "core/utils/AssetPath.h"

SDLGraphicsContext::SDLGraphicsContext(SDL_Renderer* renderer) : m_renderer(renderer) {
    if (m_renderer) {
        SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
    }
}

SDLGraphicsContext::~SDLGraphicsContext() {}

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

void SDLGraphicsContext::draw_text(const std::string& text, float x, float y, const TextStyle& style) {
    if (!m_renderer) {
        return;
    }

    // Reuse the layout metrics logic to avoid duplicating measurement behavior.
    TextMetrics metrics = measure_text(text, style);
    int target_width = static_cast<int>(std::ceil(metrics.width));
    int target_height = static_cast<int>(std::ceil(metrics.height));
    if (target_width <= 0 || target_height <= 0) {
        std::cerr << "[draw_text] measured zero size for '" << text << "'\n";
        return;
    }
    if (m_viewport.width > 0 && m_viewport.height > 0) {
        if (y + target_height < m_viewport.y || y > m_viewport.y + m_viewport.height) {
            return;
        }
        if (x + target_width < m_viewport.x || x > m_viewport.x + m_viewport.width) {
            return;
        }
    }

    auto resolved_font = Hummingbird::resolve_asset_path(style.font_path);

    BLFontFace face;
    if (face.createFromFile(resolved_font.string().c_str()) != BL_SUCCESS) {
        std::cerr << "Failed to load font: " << resolved_font << std::endl;
        return;
    }

    BLFont font;
    font.createFromFace(face, style.font_size);

    BLFontMetrics fm = font.metrics();

    BLImage img(target_width, target_height, BL_FORMAT_PRGB32);
    BLContext ctx(img);

    // Clear to transparent; text will be blended over the target.
    ctx.clearAll();

    ctx.setFillStyle(BLRgba32(style.color.r, style.color.g, style.color.b, style.color.a));
    double baseline_y = fm.ascent;  // place baseline inside the image
    ctx.fillUtf8Text(BLPoint(0.0, baseline_y), font, text.c_str());
    if (style.bold) {
        ctx.fillUtf8Text(BLPoint(0.5, baseline_y), font, text.c_str());
    }
    ctx.end();

    BLImageData imgData;
    img.getData(&imgData);
    std::span<const uint8_t> pixels{static_cast<const uint8_t*>(imgData.pixelData),
                                    static_cast<size_t>(imgData.stride) * static_cast<size_t>(target_height)};

    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormatFrom(
        const_cast<uint8_t*>(pixels.data()), target_width, target_height, 32, imgData.stride, SDL_PIXELFORMAT_BGRA32);
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

    SDL_Rect dest_rect = {(int)x, (int)y, target_width, target_height};

    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);

    static bool logged = false;
    if (!logged) {
        std::cerr << "[draw_text] text='" << text << "' at (" << x << ", " << y << ") size=(" << target_width << ", "
                  << target_height << ") font=" << resolved_font << "\n";
        logged = true;
    }

    SDL_RenderCopy(m_renderer, texture, NULL, &dest_rect);

    SDL_DestroyTexture(texture);
}

TextMetrics SDLGraphicsContext::measure_text(const std::string& text, const TextStyle& style) {
    if (text.empty()) {
        return {0, 0};
    }

    auto resolved_font = Hummingbird::resolve_asset_path(style.font_path);

    BLFontFace face;

    BLResult err = face.createFromFile(resolved_font.string().c_str());

    if (err != BL_SUCCESS) {
        std::cerr << "Failed to load font: " << resolved_font << " (err=" << err << ")" << std::endl;

        return {0, 0};
    }

    BLFont font;

    font.createFromFace(face, style.font_size);

    BLGlyphBuffer glyphBuffer;

    glyphBuffer.setUtf8Text(text.c_str());

    font.shape(glyphBuffer);

    BLTextMetrics tm;
    font.getTextMetrics(glyphBuffer, tm);

    // Prefer advance width but guard with bounding box to avoid clipping.
    float width = static_cast<float>(tm.advance.x);
    float bbox_width = static_cast<float>(tm.boundingBox.x1 - tm.boundingBox.x0);
    if (width <= 0 && bbox_width > 0) {
        width = bbox_width;
    } else if (bbox_width > width) {
        width = bbox_width;
    }

    // Simple approximations for bold/italic when only a regular font is available.
    if (style.bold) width += 1.0f;
    if (style.italic) width += 1.0f;

    // Use font metrics for a consistent line height with a small fudge for descenders.
    BLFontMetrics fm = font.metrics();
    float height = fm.ascent + fm.descent + 1.0f;  // small pad to prevent clipping

    static bool logged = false;
    if (!logged) {
        std::cerr << "[measure_text] path=" << resolved_font << " text='" << text << "' size=" << style.font_size
                  << " -> (" << width << ", " << height << ")\n";
        logged = true;
    }

    return {width, height};
}
