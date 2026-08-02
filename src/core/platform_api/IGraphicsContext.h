#pragma once

#include <cstdint>
#include <string>

#include "core/GraphicsTypes.h"
#include "core/ResourceRef.h"
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
    // Two ways to draw an image, for two genuinely different lifetimes
    // (T-RESOURCE-REF-1).
    //
    // By REFERENCE, for a resource the engine's store owns: the pixels are
    // resolved here, at the last possible moment, so nothing upstream — not the
    // render tree, not a retained display list — holds a pointer that the store
    // can free underneath it. A ref that no longer resolves draws nothing.
    virtual void draw_image(ResourceRef image, const Hummingbird::Geometry::Rect& dest) = 0;
    // By VALUE, for a bitmap whose owner provably outlives the call: an inline
    // <svg> rasterized into the render object itself, or a chrome icon owned by
    // the app. These were never at risk and routing them through the store would
    // be generality for its own sake.
    virtual void draw_image(const ImageBitmap& image, const Hummingbird::Geometry::Rect& dest) = 0;

    // Supplies the resolver used by the reference overload. The engine owns the
    // store and injects it; contexts that never draw resource images (tests,
    // recorders that only forward the ref) can ignore it.
    virtual void set_resource_resolver(const IResourceResolver* /*resolver*/) {}
    // Layout needs it too: a replaced element's intrinsic size comes from the
    // pixels, so sizing resolves the same handle painting does. The pointer is
    // read and dropped within the call, never stored.
    virtual const IResourceResolver* resource_resolver() const { return nullptr; }
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
