#pragma once

#include <string>

// Dependency-free graphics value types shared across the codebase. These used to
// live in core/platform_api/IGraphicsContext.h, which meant every consumer of a
// plain `Color` (the entire style layer, text utilities, ...) had to depend on
// the platform *graphics port* interface. Splitting them out keeps the port a
// true leaf: only real graphics drivers (layout text measurement, renderer,
// engine, platform adapters) include IGraphicsContext.h itself (T-CORE-GFXTYPES-1).

namespace Hummingbird {

// Defined by the platform image layer (core/platform_api/IImageDecoder.h); only
// referenced by pointer/reference here, so a forward declaration suffices.
struct ImageBitmap;

struct Color {
    unsigned char r, g, b, a;
};

struct TextMetrics {
    float width = 0.0f;
    float height = 0.0f;
    float ascent = 0.0f;
    float descent = 0.0f;
    float underline_position = 0.0f;
    float underline_thickness = 0.0f;
};

struct TextStyle {
    std::string font_path;
    float font_size = 16.0f;
    bool bold = false;
    bool italic = false;
    bool monospace = false;
    Color color{0, 0, 0, 255};
};

}  // namespace Hummingbird
