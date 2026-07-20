#include "core/utils/Url.h"

#include <stddef.h>

#include "core/utils/ParseUtils.h"
#include "core/utils/StringUtils.h"

namespace Hummingbird::Core {

namespace {

bool has_scheme(std::string_view url) {
    return url.find("://") != std::string_view::npos;
}

// Case-insensitive check that |url| begins with |prefix| (an ASCII scheme like
// "javascript:").
bool starts_with_ci(std::string_view url, std::string_view lower_prefix) {
    if (url.size() < lower_prefix.size()) return false;
    for (size_t i = 0; i < lower_prefix.size(); ++i) {
        char c = url[i];
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
        if (c != lower_prefix[i]) return false;
    }
    return true;
}

// Pseudo-schemes that must never be resolved as a relative path: they are opaque
// (no authority/path to join against a base). Curated rather than "any scheme"
// so an ambiguous bare `host:port` href is left to the existing logic.
bool is_opaque_scheme_url(std::string_view url) {
    for (std::string_view scheme : {"javascript:", "mailto:", "tel:", "data:", "about:", "blob:"}) {
        if (starts_with_ci(url, scheme)) return true;
    }
    return false;
}

std::string normalize_path(std::string_view path) {
    std::string out;
    out.reserve(path.size());

    bool absolute = !path.empty() && path.front() == '/';
    const bool preserve_trailing_slash = path.size() > 1 && path.back() == '/';
    size_t i = 0;
    while (i < path.size()) {
        while (i < path.size() && path[i] == '/') ++i;
        size_t start = i;
        while (i < path.size() && path[i] != '/') ++i;
        std::string_view segment = path.substr(start, i - start);
        if (segment.empty() || segment == ".") {
            continue;
        }
        if (segment == "..") {
            if (!out.empty()) {
                size_t slash = out.find_last_of('/');
                if (slash == std::string::npos) {
                    out.clear();
                } else {
                    out.erase(slash);
                }
            }
            continue;
        }
        out.push_back('/');
        out.append(segment);
    }

    if (out.empty()) {
        return absolute ? "/" : std::string{};
    }
    if (preserve_trailing_slash && out.back() != '/') {
        out.push_back('/');
    }
    return out;
}

std::string base_dir(std::string_view path) {
    if (path.empty()) return "/";
    size_t slash = path.find_last_of('/');
    if (slash == std::string_view::npos) return "/";
    if (slash == 0) return "/";
    return std::string(path.substr(0, slash + 1));
}

}  // namespace

std::optional<UrlParts> parse_absolute_url(std::string_view url) {
    url = Utils::trim_ascii_whitespace(url);
    const size_t scheme_pos = url.find("://");
    if (scheme_pos == std::string_view::npos) return std::nullopt;

    UrlParts out;
    out.scheme = Utils::to_lower(url.substr(0, scheme_pos));
    std::string_view rest = url.substr(scheme_pos + 3);
    if (rest.empty()) return std::nullopt;

    size_t path_pos = rest.find('/');
    std::string_view authority = path_pos == std::string_view::npos ? rest : rest.substr(0, path_pos);
    out.path = path_pos == std::string_view::npos ? "/" : std::string(rest.substr(path_pos));

    size_t port_pos = authority.find(':');
    if (port_pos == std::string_view::npos) {
        out.host = Utils::to_lower(authority);
    } else {
        out.host = Utils::to_lower(authority.substr(0, port_pos));
        std::string_view port_str = authority.substr(port_pos + 1);
        if (!port_str.empty()) {
            auto parsed_port = Utils::parse_long(port_str, Utils::NumberParseMode::Strict);
            if (!parsed_port.has_value()) {
                return std::nullopt;
            }
            if (*parsed_port > 0 && *parsed_port <= 65535) {
                out.port = static_cast<uint16_t>(*parsed_port);
            }
        }
    }

    if (out.host.empty()) return std::nullopt;
    return out;
}

std::string normalize_input_url(std::string_view input) {
    input = Utils::trim_ascii_whitespace(input);
    if (input.empty()) return {};

    // Opaque pseudo-schemes (about:bookmarks, mailto:, ...) are kept verbatim
    // rather than treated as a bare host to prefix with https://.
    if (is_opaque_scheme_url(input)) {
        return std::string(input);
    }

    if (input.rfind("//", 0) == 0) {
        std::string normalized("https:");
        normalized.append(input);
        return normalized;
    }

    if (has_scheme(input)) {
        return std::string(input);
    }

    // If it's a bare host/path, default to https.
    std::string normalized("https://");
    normalized.append(input);
    return normalized;
}

std::string resolve_url(std::string_view base_url, std::string_view href) {
    href = Utils::trim_ascii_whitespace(href);
    if (href.empty()) return {};

    // Opaque pseudo-schemes (javascript:, mailto:, ...) are not resolved against
    // the base — they carry no path to join. Returned verbatim so callers can
    // recognize them (e.g. skip navigating a javascript: link).
    if (is_opaque_scheme_url(href)) {
        return std::string(href);
    }

    if (has_scheme(href)) {
        return std::string(href);
    }

    auto base = parse_absolute_url(base_url);
    if (!base) {
        return std::string(href);
    }

    if (href.rfind("//", 0) == 0) {
        return base->scheme + ":" + std::string(href);
    }

    if (href.front() == '/') {
        std::string path = normalize_path(href);
        std::string resolved = base->scheme + "://" + base->host;
        if (base->port) {
            resolved += ":" + std::to_string(*base->port);
        }
        resolved += path;
        return resolved;
    }

    if (href.front() == '#' || href.front() == '?') {
        std::string resolved = base->scheme + "://" + base->host;
        if (base->port) {
            resolved += ":" + std::to_string(*base->port);
        }
        resolved += base->path;
        resolved += std::string(href);
        return resolved;
    }

    std::string dir = base_dir(base->path);
    std::string combined = dir + std::string(href);
    std::string path = normalize_path(combined);

    std::string resolved = base->scheme + "://" + base->host;
    if (base->port) {
        resolved += ":" + std::to_string(*base->port);
    }
    resolved += path.empty() ? "/" : path;
    return resolved;
}

std::string_view url_fragment(std::string_view url) {
    const auto pos = url.find('#');
    return pos == std::string_view::npos ? std::string_view{} : url.substr(pos);
}

std::string_view url_without_fragment(std::string_view url) {
    const auto pos = url.find('#');
    return pos == std::string_view::npos ? url : url.substr(0, pos);
}

bool is_javascript_url(std::string_view url) {
    return starts_with_ci(Utils::trim_ascii_whitespace(url), "javascript:");
}

}  // namespace Hummingbird::Core
