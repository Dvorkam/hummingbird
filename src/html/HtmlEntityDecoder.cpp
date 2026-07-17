#include "html/HtmlEntityDecoder.h"

#include <cstdint>

namespace Hummingbird::Html::Utils {

namespace {

constexpr size_t kMaxEntityBodyLength = 32;

struct NamedEntity {
    std::string_view name;
    std::string_view utf8;
};

// UTF-8 encodings are written as explicit byte escapes so the compiler's
// execution charset can never mangle them (MSVC without /utf-8 re-encodes
// \uXXXX narrow literals into the local codepage).
constexpr NamedEntity kNamedEntities[] = {
    {"amp", "&"},
    {"lt", "<"},
    {"gt", ">"},
    {"quot", "\""},
    {"apos", "'"},
    {"nbsp", "\xC2\xA0"},
    {"middot", "\xC2\xB7"},
    {"sect", "\xC2\xA7"},
    {"para", "\xC2\xB6"},
    {"laquo", "\xC2\xAB"},
    {"raquo", "\xC2\xBB"},
    {"copy", "\xC2\xA9"},
    {"reg", "\xC2\xAE"},
    {"deg", "\xC2\xB0"},
    {"plusmn", "\xC2\xB1"},
    {"times", "\xC3\x97"},
    {"divide", "\xC3\xB7"},
    {"ndash", "\xE2\x80\x93"},
    {"mdash", "\xE2\x80\x94"},
    {"lsquo", "\xE2\x80\x98"},
    {"rsquo", "\xE2\x80\x99"},
    {"ldquo", "\xE2\x80\x9C"},
    {"rdquo", "\xE2\x80\x9D"},
    {"bull", "\xE2\x80\xA2"},
    {"hellip", "\xE2\x80\xA6"},
    {"euro", "\xE2\x82\xAC"},
    {"trade", "\xE2\x84\xA2"},
    {"larr", "\xE2\x86\x90"},
    {"uarr", "\xE2\x86\x91"},
    {"rarr", "\xE2\x86\x92"},
    {"darr", "\xE2\x86\x93"},
    {"harr", "\xE2\x86\x94"},
};

void append_utf8(std::string& out, uint32_t codepoint) {
    if (codepoint == 0 || codepoint > 0x10FFFF || (codepoint >= 0xD800 && codepoint <= 0xDFFF)) {
        out.append("\xEF\xBF\xBD");  // U+FFFD replacement character
        return;
    }
    if (codepoint < 0x80) {
        out.push_back(static_cast<char>(codepoint));
    } else if (codepoint < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (codepoint >> 6)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else if (codepoint < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | (codepoint >> 12)));
        out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (codepoint >> 18)));
        out.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    }
}

// Decodes `#123` / `#x1F3` numeric entity bodies; returns false if malformed.
bool decode_numeric_entity(std::string_view body, std::string& out) {
    if (body.size() < 2 || body[0] != '#') {
        return false;
    }
    size_t i = 1;
    const bool hex = body[1] == 'x' || body[1] == 'X';
    if (hex) {
        i = 2;
    }
    if (i >= body.size()) {
        return false;
    }
    uint32_t codepoint = 0;
    for (; i < body.size(); ++i) {
        const char c = body[i];
        uint32_t digit = 0;
        if (c >= '0' && c <= '9') {
            digit = static_cast<uint32_t>(c - '0');
        } else if (hex && c >= 'a' && c <= 'f') {
            digit = 10u + static_cast<uint32_t>(c - 'a');
        } else if (hex && c >= 'A' && c <= 'F') {
            digit = 10u + static_cast<uint32_t>(c - 'A');
        } else {
            return false;
        }
        codepoint = codepoint * (hex ? 16u : 10u) + digit;
        if (codepoint > 0x10FFFF) {
            codepoint = 0x110000;  // overflow clamp; emits U+FFFD below
        }
    }
    append_utf8(out, codepoint);
    return true;
}

bool decode_named_entity(std::string_view name, std::string& out) {
    for (const auto& entity : kNamedEntities) {
        if (name == entity.name) {
            out.append(entity.utf8);
            return true;
        }
    }
    return false;
}

}  // namespace

std::string decode_named_entities(std::string_view text) {
    if (text.find('&') == std::string_view::npos) {
        return std::string(text);
    }
    std::string decoded;
    decoded.reserve(text.size());
    size_t i = 0;
    while (i < text.size()) {
        if (text[i] != '&') {
            decoded.push_back(text[i]);
            ++i;
            continue;
        }
        size_t semi = text.find(';', i + 1);
        if (semi == std::string_view::npos || semi - i - 1 > kMaxEntityBodyLength) {
            decoded.push_back(text[i]);
            ++i;
            continue;
        }
        std::string_view body = text.substr(i + 1, semi - i - 1);
        if (decode_named_entity(body, decoded) || decode_numeric_entity(body, decoded)) {
            i = semi + 1;
            continue;
        }
        // Unknown entity: keep the original text.
        decoded.append(text.substr(i, semi - i + 1));
        i = semi + 1;
    }
    return decoded;
}

}  // namespace Hummingbird::Html::Utils
