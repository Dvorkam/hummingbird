#pragma once

#include <blend2d/font.h>
#include <blend2d/fontface.h>

#include <mutex>
#include <string>
#include <unordered_map>

namespace Hummingbird::Platform {

struct FontSetup {
    BLFontFace face;
    BLFont font;
    BLFontMetrics metrics;
};

class FontCache {
public:
    static FontCache& instance();

    const FontSetup* get_or_load(const std::string& font_path, float font_size, bool include_error);
    void set_max_entries(size_t max_entries);

private:
    struct FontKey {
        std::string path;
        float size = 0.0f;

        bool operator==(const FontKey& other) const { return size == other.size && path == other.path; }
    };

    struct FontKeyHash {
        size_t operator()(const FontKey& key) const;
    };

    struct Entry {
        FontSetup setup;
        size_t last_used = 0;
    };

    FontCache() = default;

    bool load_font_setup(const std::string& font_path, float font_size, FontSetup& out, bool include_error);
    void evict_if_needed();

    std::mutex mutex_;
    std::unordered_map<FontKey, Entry, FontKeyHash> entries_;
    size_t tick_ = 0;
    size_t max_entries_ = 32;
};

}  // namespace Hummingbird::Platform
