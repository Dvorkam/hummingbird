#include "platform/decoders/SvgImageDecoder.h"

#include <lunasvg/lunasvg.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>

#include "platform/decoders/ImageDecodeUtils.h"

namespace Hummingbird::Platform {

namespace {
constexpr int kDefaultSvgWidth = 300;
constexpr int kDefaultSvgHeight = 150;
constexpr size_t kScanLimit = 1024;

bool starts_with_ci(std::string_view text, std::string_view prefix) {
    if (text.size() < prefix.size()) {
        return false;
    }
    for (size_t i = 0; i < prefix.size(); ++i) {
        const auto lhs = static_cast<unsigned char>(text[i]);
        const auto rhs = static_cast<unsigned char>(prefix[i]);
        if (std::tolower(lhs) != std::tolower(rhs)) {
            return false;
        }
    }
    return true;
}

void skip_whitespace(std::string_view& text) {
    while (!text.empty()) {
        unsigned char ch = static_cast<unsigned char>(text.front());
        if (!std::isspace(ch)) {
            break;
        }
        text.remove_prefix(1);
    }
}

bool looks_like_svg(std::string_view bytes) {
    if (bytes.empty()) {
        return false;
    }

    if (bytes.size() >= 3 && static_cast<unsigned char>(bytes[0]) == 0xEF &&
        static_cast<unsigned char>(bytes[1]) == 0xBB && static_cast<unsigned char>(bytes[2]) == 0xBF) {
        bytes.remove_prefix(3);
    }

    skip_whitespace(bytes);
    if (bytes.empty()) {
        return false;
    }

    if (bytes.front() != '<') {
        return false;
    }

    if (starts_with_ci(bytes, "<svg")) {
        return true;
    }

    // An SVG file does not have to open with its root element. Three prologues
    // are ordinary and all of them were being rejected here, which sent real
    // SVG icons on to SDL_image's much weaker parser — 830 "Couldn't parse SVG
    // image" warnings in one browsing sweep, i.e. missing icons across most of
    // the modern web.
    //
    // `<!DOCTYPE svg` is matched rather than any doctype ON PURPOSE: an HTML
    // page beginning `<!DOCTYPE html>` very often contains an inline `<svg>`
    // icon, and accepting that would make us decode a whole document as an
    // image. The doctype has to name svg itself.
    const bool xml_declaration = starts_with_ci(bytes, "<?xml");
    const bool comment = starts_with_ci(bytes, "<!--");
    bool svg_doctype = false;
    if (starts_with_ci(bytes, "<!DOCTYPE")) {
        std::string_view after = bytes.substr(std::string_view("<!DOCTYPE").size());
        skip_whitespace(after);
        svg_doctype = starts_with_ci(after, "svg");
    }
    if (!xml_declaration && !comment && !svg_doctype) {
        return false;
    }

    const size_t limit = std::min(bytes.size(), kScanLimit);
    for (size_t i = 0; i + 4 <= limit; ++i) {
        if (bytes[i] != '<') {
            continue;
        }
        if (std::tolower(static_cast<unsigned char>(bytes[i + 1])) == 's' &&
            std::tolower(static_cast<unsigned char>(bytes[i + 2])) == 'v' &&
            std::tolower(static_cast<unsigned char>(bytes[i + 3])) == 'g') {
            return true;
        }
    }

    return false;
}

int resolve_dimension(float primary, float fallback, int default_value) {
    float value = primary;
    if (!(value > 0.0f) || !std::isfinite(value)) {
        value = fallback;
    }
    if (!(value > 0.0f) || !std::isfinite(value)) {
        value = static_cast<float>(default_value);
    }
    int result = static_cast<int>(std::ceil(value));
    if (result <= 0) {
        result = default_value;
    }
    return result;
}
}  // namespace

std::optional<ImageBitmap> SvgImageDecoder::decode(std::string_view bytes) {
    if (!looks_like_svg(bytes)) {
        return std::nullopt;
    }

    auto document = lunasvg::Document::loadFromData(bytes.data(), bytes.size());
    if (!document) {
        return std::nullopt;
    }

    const auto box = document->boundingBox();
    const int width = resolve_dimension(document->width(), box.w, kDefaultSvgWidth);
    const int height = resolve_dimension(document->height(), box.h, kDefaultSvgHeight);

    auto bitmap = document->renderToBitmap(width, height, 0x00000000);
    if (bitmap.isNull()) {
        return std::nullopt;
    }

    bitmap.convertToRGBA();

    auto stride = checked_stride(width, 4);
    if (!stride) {
        return std::nullopt;
    }
    auto out = allocate_bitmap(width, height, *stride, PixelFormat::BGRA32);
    if (!out) {
        return std::nullopt;
    }

    const uint8_t* src = bitmap.data();
    const int src_stride = bitmap.stride();
    for (int y = 0; y < height; ++y) {
        const uint8_t* src_row = src + y * src_stride;
        uint8_t* dst_row = out->pixels.data() + static_cast<size_t>(y) * static_cast<size_t>(out->stride);
        for (int x = 0; x < width; ++x) {
            const size_t idx = static_cast<size_t>(x) * 4;
            dst_row[idx + 0] = src_row[idx + 2];
            dst_row[idx + 1] = src_row[idx + 1];
            dst_row[idx + 2] = src_row[idx + 0];
            dst_row[idx + 3] = src_row[idx + 3];
        }
    }

    return out;
}

}  // namespace Hummingbird::Platform
