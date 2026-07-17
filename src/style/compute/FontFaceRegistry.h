#pragma once

#include <string>
#include <string_view>
#include <unordered_map>

#include "core/utils/StringUtils.h"

namespace Hummingbird::Css {

// Maps a normalized (@font-face) family name to the resolved font key that the
// platform font cache can load: a bundled asset path for local sources, or the
// remote font url whose bytes have been registered with the cache. Only faces
// with a *loadable* source (raw TTF/OTF, not WOFF/WOFF2 yet) are registered, so
// a hit here always means a usable font.
//
// Populated by the document/resource layer after @font-face rules are collected
// and their sources resolved; consulted by the StyleEngine at compute time to
// stamp ComputedStyle::font_src.
class FontFaceRegistry {
public:
    void register_family(std::string normalized_family, std::string resolved_key) {
        families_[std::move(normalized_family)] = std::move(resolved_key);
    }

    void clear() { families_.clear(); }
    bool empty() const { return families_.empty(); }
    size_t size() const { return families_.size(); }

    // Given a computed font-family list (comma-separated, lowercased, single-
    // space-joined — the same shape parse_font_family_list produces), return the
    // resolved key of the first family with a registered @font-face, else "".
    std::string resolve(std::string_view font_family_list) const {
        size_t start = 0;
        while (start <= font_family_list.size()) {
            size_t comma = font_family_list.find(',', start);
            std::string_view segment = comma == std::string_view::npos ? font_family_list.substr(start)
                                                                       : font_family_list.substr(start, comma - start);
            std::string_view trimmed = Core::Utils::trim_ascii_whitespace(segment);
            if (!trimmed.empty()) {
                auto it = families_.find(std::string(trimmed));
                if (it != families_.end()) {
                    return it->second;
                }
            }
            if (comma == std::string_view::npos) {
                break;
            }
            start = comma + 1;
        }
        return {};
    }

private:
    std::unordered_map<std::string, std::string> families_;
};

}  // namespace Hummingbird::Css
