#include "core/utils/Url.h"

#include <stddef.h>

#include "core/utils/StringUtils.h"

namespace Hummingbird::Core {

namespace {

bool has_scheme(std::string_view url) {
    return url.find("://") != std::string_view::npos;
}

std::string normalize_path(std::string_view path) {
    std::string out;
    out.reserve(path.size());

    bool absolute = !path.empty() && path.front() == '/';
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
            try {
                int port = std::stoi(std::string(port_str));
                if (port > 0 && port <= 65535) {
                    out.port = static_cast<uint16_t>(port);
                }
            } catch (...) {
                return std::nullopt;
            }
        }
    }

    if (out.host.empty()) return std::nullopt;
    return out;
}

std::string normalize_input_url(std::string_view input) {
    input = Utils::trim_ascii_whitespace(input);
    if (input.empty()) return {};

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

}  // namespace Hummingbird::Core
