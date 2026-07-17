#pragma once

#include <cstdint>
#include <string>

#include "core/GraphicsTypes.h"
#include "core/geometry/Geometry.h"

namespace Hummingbird {

// Color / TextMetrics / TextStyle / ImageBitmap now live in core/GraphicsTypes.h
// so consumers that only need the value types do not depend on this port.

class IGraphicsContext {
public:
    virtual ~IGraphicsContext() = default;

    virtual void set_viewport(const Hummingbird::Geometry::Rect& viewport) = 0;
    virtual void clear(const Color& color) = 0;
    virtual void present() = 0;
    virtual void fill_rect(const Hummingbird::Geometry::Rect& rect, const Color& color) = 0;
    virtual void draw_image(const ImageBitmap& image, const Hummingbird::Geometry::Rect& dest) = 0;
    virtual TextMetrics measure_text(const std::string& text, const TextStyle& style) = 0;
    virtual void draw_text(const std::string& text, float x, float y, const TextStyle& style) = 0;
    virtual void draw_text_with_metrics(const std::string& text, float x, float y, const TextStyle& style,
                                        const TextMetrics& metrics) {
        draw_text(text, x, y, style);
    }
    virtual void set_global_alpha(float /*alpha*/) {}
    virtual void set_text_cache_owner(std::uint64_t /*owner_id*/) {}

    // Restrict subsequent drawing to the intersection of `rect` with the current
    // clip, until the matching pop_clip(). Implementations without clipping
    // ignore both (drawing is simply not clipped). Used for background-clip and,
    // later, overflow:hidden.
    virtual void push_clip(const Hummingbird::Geometry::Rect& /*rect*/) {}
    virtual void pop_clip() {}

    // Optional document cache hooks for partial redraws.
    virtual bool begin_document_cache(const Hummingbird::Geometry::Rect& /*viewport*/) { return false; }
    virtual void end_document_cache() {}
    virtual void draw_document_cache() {}
};

}  // namespace Hummingbird
