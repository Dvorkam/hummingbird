#include "platform/SDLGraphicsContext.h"

#include <SDL.h>
#include <blend2d.h>

#include <cmath>
#include <span>

#include "core/utils/AssetPath.h"
#include "core/utils/Log.h"

namespace {
struct FontSetup {
    BLFontFace face;
    BLFont font;
    BLFontMetrics metrics;
};

bool load_font_setup(const std::string& font_path, float font_size, FontSetup& out, bool include_error) {
    BLResult err = out.face.createFromFile(font_path.c_str());
    if (err != BL_SUCCESS) {
        if (include_error) {
            HB_LOG_ERROR("[platform] Failed to load font: " << font_path << " (err=" << err << ")");
        } else {
            HB_LOG_ERROR("[platform] Failed to load font: " << font_path);
        }
        return false;
    }

    out.font.createFromFace(out.face, font_size);
    out.metrics = out.font.metrics();
    return true;
}

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
    int target_width = 0;
    int target_height = 0;
    if (!resolve_target_dimensions(metrics, target_width, target_height)) {
        HB_LOG_DEBUG("[draw_text] measured zero size for '" << text << "'");
        return;
    }
    if (is_outside_viewport(m_viewport, x, y, target_width, target_height)) return;

    auto resolved_font = Hummingbird::resolve_asset_path(style.font_path).string();
    FontSetup font_setup;
    if (!load_font_setup(resolved_font, style.font_size, font_setup, false)) {
        return;
    }

    SDL_Texture* texture = build_text_texture(m_renderer, text, style, font_setup, target_width, target_height);
    if (!texture) return;

    SDL_Rect dest_rect = {(int)x, (int)y, target_width, target_height};

    static bool logged = false;
    if (!logged) {
        HB_LOG_DEBUG("[draw_text] text='" << text << "' at (" << x << ", " << y << ") size=(" << target_width << ", "
                                         << target_height << ") font=" << resolved_font);
        logged = true;
    }

    SDL_RenderCopy(m_renderer, texture, NULL, &dest_rect);

    SDL_DestroyTexture(texture);
}

TextMetrics SDLGraphicsContext::measure_text(const std::string& text, const TextStyle& style) {
    if (text.empty()) {
        return {0, 0};
    }

    auto resolved_font = Hummingbird::resolve_asset_path(style.font_path).string();
    FontSetup font_setup;
    if (!load_font_setup(resolved_font, style.font_size, font_setup, true)) {
        return {0, 0};
    }

    BLGlyphBuffer glyphBuffer;
    glyphBuffer.setUtf8Text(text.c_str());
    font_setup.font.shape(glyphBuffer);

    BLTextMetrics tm;
    font_setup.font.getTextMetrics(glyphBuffer, tm);

    // Prefer advance width but guard with bounding box to avoid clipping.
    float width = compute_text_width(tm);

    // Simple approximations for bold/italic when only a regular font is available.
    if (style.bold) width += 1.0f;
    if (style.italic) width += 1.0f;

    // Use font metrics for a consistent line height with a small fudge for descenders.
    float height = compute_text_height(font_setup.metrics);

    static bool logged = false;
    if (!logged) {
        HB_LOG_DEBUG("[measure_text] path=" << resolved_font << " text='" << text << "' size=" << style.font_size
                                            << " -> (" << width << ", " << height << ")");
        logged = true;
    }

    return {width, height};
}
