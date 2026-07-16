#pragma once

#include <string>
#include <vector>

#include "style/compute/FontFaceRegistry.h"
#include "style/compute/Stylesheet.h"

namespace Hummingbird::Engine {

// Turns parsed @font-face rules into a registry of *loadable* font families and
// reports remote font urls that still need fetching. Local (bundled asset)
// sources resolve immediately; remote sources resolve once their bytes have been
// fetched into the resource store and written to the on-disk font cache. A face
// whose only sources are undecodable (WOFF/WOFF2 until T-FONT-WOFF2-1) is left
// unregistered, so text falls back to the built-in family mapping.
class IFontFaceResolver {
public:
    virtual ~IFontFaceResolver() = default;

    virtual Css::FontFaceRegistry resolve_font_faces(const std::vector<Css::FontFaceRule>& faces,
                                                     std::vector<std::string>& out_pending_remote) const = 0;
};

}  // namespace Hummingbird::Engine
