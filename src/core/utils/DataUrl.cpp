#include "core/utils/DataUrl.h"

#include <array>
#include <cctype>
#include <cstdint>

namespace Hummingbird::Core::Utils {

namespace {

constexpr std::string_view kScheme = "data:";

bool equals_ci(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

std::string to_lower_copy(std::string_view input) {
    std::string out;
    out.reserve(input.size());
    for (char c : input) {
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return out;
}

std::string_view trim(std::string_view input) {
    while (!input.empty() && std::isspace(static_cast<unsigned char>(input.front()))) input.remove_prefix(1);
    while (!input.empty() && std::isspace(static_cast<unsigned char>(input.back()))) input.remove_suffix(1);
    return input;
}

int hex_value(unsigned char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// Percent-decoding that leaves anything which is not a well-formed escape
// exactly as it was. A hand-written inline SVG is full of characters a strict
// URL parser would reject, and rejecting the image is worse than accepting the
// bytes the author clearly meant.
std::string percent_decode(std::string_view input) {
    std::string out;
    out.reserve(input.size());
    for (size_t i = 0; i < input.size(); ++i) {
        if (input[i] == '%' && i + 2 < input.size()) {
            const int high = hex_value(static_cast<unsigned char>(input[i + 1]));
            const int low = hex_value(static_cast<unsigned char>(input[i + 2]));
            if (high >= 0 && low >= 0) {
                out.push_back(static_cast<char>((high << 4) | low));
                i += 2;
                continue;
            }
        }
        out.push_back(input[i]);
    }
    return out;
}

// Reverse base64 alphabet: index by byte, -1 for "not a base64 character".
const std::array<int8_t, 256>& base64_reverse() {
    static const std::array<int8_t, 256> table = [] {
        std::array<int8_t, 256> t{};
        t.fill(-1);
        constexpr std::string_view kAlphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        for (size_t i = 0; i < kAlphabet.size(); ++i) {
            t[static_cast<unsigned char>(kAlphabet[i])] = static_cast<int8_t>(i);
        }
        // base64url, which shows up in hand-written data URLs often enough to
        // accept: the decoded bytes are identical either way.
        t[static_cast<unsigned char>('-')] = 62;
        t[static_cast<unsigned char>('_')] = 63;
        return t;
    }();
    return table;
}

// Decodes base64, ignoring whitespace and tolerating absent padding. Returns
// nullopt only on a character that cannot belong to base64 at all, which means
// the URL is not what it claimed to be.
std::optional<std::string> base64_decode(std::string_view input) {
    const auto& reverse = base64_reverse();
    std::string out;
    out.reserve(input.size() / 4 * 3 + 3);

    uint32_t accumulator = 0;
    int bits = 0;
    for (unsigned char c : input) {
        if (std::isspace(c)) continue;
        if (c == '=') break;  // padding: everything after it is padding too
        const int8_t value = reverse[c];
        if (value < 0) {
            return std::nullopt;
        }
        accumulator = (accumulator << 6) | static_cast<uint32_t>(value);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<char>((accumulator >> bits) & 0xFF));
        }
    }
    // Leftover bits are the tail of a truncated group. They encode no whole
    // byte, so dropping them is what a lenient decoder does.
    return out;
}

}  // namespace

bool is_data_url(std::string_view url) {
    return url.size() >= kScheme.size() && equals_ci(url.substr(0, kScheme.size()), kScheme);
}

std::optional<DataUrl> parse_data_url(std::string_view url) {
    if (!is_data_url(url)) {
        return std::nullopt;
    }
    std::string_view rest = url.substr(kScheme.size());

    // The FIRST comma separates metadata from payload. Later commas belong to the
    // payload — an inline SVG path is nothing but commas.
    const size_t comma = rest.find(',');
    if (comma == std::string_view::npos) {
        return std::nullopt;  // not a data URL at all without one
    }

    std::string_view metadata = trim(rest.substr(0, comma));
    std::string_view payload = rest.substr(comma + 1);

    bool is_base64 = false;
    constexpr std::string_view kBase64 = ";base64";
    if (metadata.size() >= kBase64.size() && equals_ci(metadata.substr(metadata.size() - kBase64.size()), kBase64)) {
        is_base64 = true;
        metadata.remove_suffix(kBase64.size());
    }

    DataUrl result;
    // Split the mediatype from its parameters; only `charset` is worth keeping.
    std::string_view mime = metadata;
    if (const size_t semi = metadata.find(';'); semi != std::string_view::npos) {
        mime = metadata.substr(0, semi);
        std::string_view params = metadata.substr(semi + 1);
        while (!params.empty()) {
            const size_t next = params.find(';');
            std::string_view param = trim(next == std::string_view::npos ? params : params.substr(0, next));
            if (const size_t eq = param.find('='); eq != std::string_view::npos) {
                if (equals_ci(trim(param.substr(0, eq)), "charset")) {
                    result.charset = to_lower_copy(trim(param.substr(eq + 1)));
                }
            }
            if (next == std::string_view::npos) break;
            params.remove_prefix(next + 1);
        }
    }
    mime = trim(mime);
    // Per spec an omitted mediatype means text/plain;charset=US-ASCII.
    result.mime_type = mime.empty() ? std::string("text/plain") : to_lower_copy(mime);
    if (mime.empty() && result.charset.empty()) {
        result.charset = "us-ascii";
    }

    if (is_base64) {
        auto decoded = base64_decode(payload);
        if (!decoded) {
            return std::nullopt;
        }
        result.data = std::move(*decoded);
    } else {
        result.data = percent_decode(payload);
    }
    return result;
}

}  // namespace Hummingbird::Core::Utils
