#include "core/net/Cors.h"

#include <algorithm>

#include "core/utils/StringUtils.h"

namespace Hummingbird::Core::Cors {

namespace {

// RFC 2616 media types a plain <form> can already produce. A Content-Type
// outside this set is what makes `application/json` preflight — the whole point
// being that a form could never have sent it, so it is a new capability.
bool is_form_content_type(std::string_view value) {
    // Strip parameters: "text/plain; charset=utf-8" is still text/plain.
    const size_t semi = value.find(';');
    std::string media = Utils::to_lower(value.substr(0, semi == std::string_view::npos ? value.size() : semi));
    // Trim surrounding whitespace left by the split.
    const size_t begin = media.find_first_not_of(" \t");
    const size_t end = media.find_last_not_of(" \t");
    if (begin == std::string::npos) return false;
    media = media.substr(begin, end - begin + 1);
    return media == "application/x-www-form-urlencoded" || media == "multipart/form-data" || media == "text/plain";
}

// Splits a comma-separated header value into lowercased, trimmed tokens.
// Access-Control-Allow-Methods/Headers are both this shape.
std::vector<std::string> split_tokens(std::string_view value) {
    std::vector<std::string> tokens;
    size_t start = 0;
    while (start <= value.size()) {
        const size_t comma = value.find(',', start);
        std::string_view token =
            value.substr(start, comma == std::string_view::npos ? value.size() - start : comma - start);
        const size_t begin = token.find_first_not_of(" \t");
        const size_t end = token.find_last_not_of(" \t");
        if (begin != std::string_view::npos) {
            tokens.push_back(Utils::to_lower(token.substr(begin, end - begin + 1)));
        }
        if (comma == std::string_view::npos) break;
        start = comma + 1;
    }
    return tokens;
}

}  // namespace

bool is_same_origin(std::string_view request_url, std::string_view document_url) {
    auto request = Origin::parse(request_url);
    auto document = Origin::parse(document_url);
    // An opaque origin matches nothing — not even itself. Treating "unparseable"
    // as same-origin would be the one mistake that disables CORS entirely.
    if (!request || !document) return false;
    return *request == *document;
}

bool is_safelisted_request_header(std::string_view name, std::string_view value) {
    const std::string lower = Utils::to_lower(name);
    if (lower == "accept" || lower == "accept-language" || lower == "content-language") {
        return true;
    }
    if (lower == "content-type") {
        return is_form_content_type(value);
    }
    return false;
}

bool is_simple_request(std::string_view method, const HttpHeaders& headers) {
    const std::string upper_method = Utils::to_upper(method);
    if (upper_method != "GET" && upper_method != "HEAD" && upper_method != "POST") {
        return false;
    }
    for (const auto& field : headers.fields()) {
        // Headers the ENGINE adds are not the page's doing and never trigger a
        // preflight: the page cannot set them through fetch, and preflighting on
        // our own User-Agent would preflight every request in the browser.
        const std::string lower = Utils::to_lower(field.name);
        if (lower == "user-agent" || lower == "referer" || lower == "origin" || lower == "cookie" ||
            lower.rfind("sec-ch-", 0) == 0) {
            continue;
        }
        if (!is_safelisted_request_header(field.name, field.value)) {
            return false;
        }
    }
    return true;
}

std::vector<std::string> headers_needing_preflight(const HttpHeaders& headers) {
    std::vector<std::string> names;
    for (const auto& field : headers.fields()) {
        const std::string lower = Utils::to_lower(field.name);
        if (lower == "user-agent" || lower == "referer" || lower == "origin" || lower == "cookie" ||
            lower.rfind("sec-ch-", 0) == 0) {
            continue;
        }
        if (is_safelisted_request_header(field.name, field.value)) {
            continue;
        }
        names.push_back(lower);
    }
    // Sorted and deduped so the emitted Access-Control-Request-Headers value does
    // not depend on header insertion order — which a preflight cache key will.
    std::sort(names.begin(), names.end());
    names.erase(std::unique(names.begin(), names.end()), names.end());
    return names;
}

Decision check_response(const HttpHeaders& response_headers, std::string_view origin, Credentials credentials) {
    const std::string_view allow_origin = response_headers.get("Access-Control-Allow-Origin");
    if (allow_origin.empty()) {
        return Decision::MissingAllowOrigin;
    }

    const bool credentialed = credentials == Credentials::Include;
    if (allow_origin == "*") {
        // `*` means "anyone may read this, and I did not look at who asked".
        // A server answering that has not decided to trust THIS user's session,
        // so honouring it for a credentialed request would hand a page another
        // site's logged-in data.
        return credentialed ? Decision::WildcardWithCredentials : Decision::Allowed;
    }
    if (!Utils::equals_ignore_case(allow_origin, origin)) {
        return Decision::OriginMismatch;
    }
    if (credentialed && !Utils::equals_ignore_case(response_headers.get("Access-Control-Allow-Credentials"), "true")) {
        return Decision::CredentialsNotAllowed;
    }
    return Decision::Allowed;
}

Decision check_preflight(const HttpHeaders& response_headers, std::string_view origin, Credentials credentials,
                         std::string_view method, const HttpHeaders& request_headers) {
    const Decision base = check_response(response_headers, origin, credentials);
    if (base != Decision::Allowed) {
        return base;
    }

    // Method. GET/HEAD/POST are always permitted by a successful preflight, so a
    // server need not list them.
    const std::string wanted = Utils::to_upper(method);
    if (wanted != "GET" && wanted != "HEAD" && wanted != "POST") {
        const std::string allowed = Utils::to_lower(response_headers.get("Access-Control-Allow-Methods"));
        const std::string needle = Utils::to_lower(wanted);
        bool found = allowed == "*";
        if (!found) {
            for (const auto& token : split_tokens(allowed)) {
                if (token == needle) {
                    found = true;
                    break;
                }
            }
        }
        if (!found) return Decision::MethodNotAllowed;
    }

    // Headers.
    const auto wanted_headers = headers_needing_preflight(request_headers);
    if (!wanted_headers.empty()) {
        const std::string allowed = Utils::to_lower(response_headers.get("Access-Control-Allow-Headers"));
        const auto allowed_tokens = split_tokens(allowed);
        const bool wildcard = allowed == "*";
        for (const std::string& name : wanted_headers) {
            if (wildcard) break;
            if (std::find(allowed_tokens.begin(), allowed_tokens.end(), name) == allowed_tokens.end()) {
                return Decision::HeaderNotAllowed;
            }
        }
    }
    return Decision::Allowed;
}

bool is_safelisted_response_header(std::string_view name) {
    const std::string lower = Utils::to_lower(name);
    // The Fetch standard's CORS-safelisted response-header names. (The milestone
    // draft listed six and omitted Content-Length; the spec has seven.)
    return lower == "cache-control" || lower == "content-language" || lower == "content-length" ||
           lower == "content-type" || lower == "expires" || lower == "last-modified" || lower == "pragma";
}

bool is_forbidden_response_header(std::string_view name) {
    const std::string lower = Utils::to_lower(name);
    // Never readable by script under any circumstances. A server that names
    // Set-Cookie in Access-Control-Expose-Headers has misunderstood, and
    // honouring it would hand the page another origin's session token.
    return lower == "set-cookie" || lower == "set-cookie2";
}

HttpHeaders filter_forbidden_response_headers(const HttpHeaders& response_headers) {
    HttpHeaders out;
    for (const auto& field : response_headers.fields()) {
        if (!is_forbidden_response_header(field.name)) {
            out.add(field.name, field.value);
        }
    }
    return out;
}

HttpHeaders filter_exposed_headers(const HttpHeaders& response_headers, Credentials credentials) {
    const std::string exposed = Utils::to_lower(response_headers.get("Access-Control-Expose-Headers"));
    const auto named = split_tokens(exposed);
    // `*` means "everything not forbidden" — but only for an anonymous request.
    // For a credentialed one the spec reads it as the literal header name "*",
    // because a server exposing everything to a logged-in caller has almost
    // certainly not thought about what "everything" contains.
    const bool wildcard =
        credentials != Credentials::Include && std::find(named.begin(), named.end(), "*") != named.end();

    HttpHeaders out;
    for (const auto& field : response_headers.fields()) {
        if (is_forbidden_response_header(field.name)) continue;
        const std::string lower = Utils::to_lower(field.name);
        const bool allowed = wildcard || is_safelisted_response_header(field.name) ||
                             std::find(named.begin(), named.end(), lower) != named.end();
        if (allowed) {
            out.add(field.name, field.value);
        }
    }
    return out;
}

std::string_view describe(Decision decision) {
    switch (decision) {
        case Decision::Allowed:
            return "allowed";
        case Decision::MissingAllowOrigin:
            return "no Access-Control-Allow-Origin on the response";
        case Decision::OriginMismatch:
            return "Access-Control-Allow-Origin names a different origin";
        case Decision::WildcardWithCredentials:
            return "Access-Control-Allow-Origin: * cannot authorize a credentialed request";
        case Decision::CredentialsNotAllowed:
            return "credentialed request without Access-Control-Allow-Credentials: true";
        case Decision::MethodNotAllowed:
            return "preflight did not allow the request method";
        case Decision::HeaderNotAllowed:
            return "preflight did not allow a request header";
    }
    return "blocked";
}

}  // namespace Hummingbird::Core::Cors
