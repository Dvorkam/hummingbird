#include "engine/extensions/ExtensionManifest.h"

#include <cctype>
#include <string>
#include <utility>

namespace Hummingbird::Engine {

namespace {
struct Cursor {
    std::string_view input;
    size_t offset = 0;

    bool eof() const { return offset >= input.size(); }
    char peek() const { return eof() ? '\0' : input[offset]; }
    void advance() {
        if (!eof()) ++offset;
    }
};

void skip_ws(Cursor& c) {
    while (!c.eof() && std::isspace(static_cast<unsigned char>(c.peek()))) {
        c.advance();
    }
}

bool match(Cursor& c, char ch) {
    skip_ws(c);
    if (c.peek() != ch) return false;
    c.advance();
    return true;
}

bool parse_literal(Cursor& c, std::string_view literal) {
    skip_ws(c);
    if (c.input.substr(c.offset, literal.size()) != literal) return false;
    c.offset += literal.size();
    return true;
}

std::optional<std::string> parse_string(Cursor& c, ManifestParseError* error) {
    skip_ws(c);
    if (!match(c, '"')) {
        if (error) {
            error->message = "Expected string";
            error->offset = c.offset;
        }
        return std::nullopt;
    }

    std::string out;
    while (!c.eof()) {
        char ch = c.peek();
        c.advance();
        if (ch == '"') {
            return out;
        }
        if (ch == '\\') {
            if (c.eof()) break;
            char esc = c.peek();
            c.advance();
            switch (esc) {
                case '"':
                case '\\':
                case '/':
                    out.push_back(esc);
                    break;
                case 'b':
                    out.push_back('\b');
                    break;
                case 'f':
                    out.push_back('\f');
                    break;
                case 'n':
                    out.push_back('\n');
                    break;
                case 'r':
                    out.push_back('\r');
                    break;
                case 't':
                    out.push_back('\t');
                    break;
                default:
                    // Keep this MVP-focused: treat other escapes as literal.
                    out.push_back(esc);
                    break;
            }
            continue;
        }
        out.push_back(ch);
    }

    if (error) {
        error->message = "Unterminated string";
        error->offset = c.offset;
    }
    return std::nullopt;
}

bool skip_value(Cursor& c, ManifestParseError* error);

bool skip_array(Cursor& c, ManifestParseError* error) {
    if (!match(c, '[')) return false;
    skip_ws(c);
    if (match(c, ']')) return true;

    while (true) {
        if (!skip_value(c, error)) return false;
        skip_ws(c);
        if (match(c, ']')) return true;
        if (match(c, ',')) continue;
        if (error) {
            error->message = "Expected ',' or ']'";
            error->offset = c.offset;
        }
        return false;
    }
}

bool skip_object(Cursor& c, ManifestParseError* error) {
    if (!match(c, '{')) return false;
    skip_ws(c);
    if (match(c, '}')) return true;

    while (true) {
        if (!parse_string(c, error)) return false;
        if (!match(c, ':')) {
            if (error) {
                error->message = "Expected ':' after object key";
                error->offset = c.offset;
            }
            return false;
        }
        if (!skip_value(c, error)) return false;
        skip_ws(c);
        if (match(c, '}')) return true;
        if (match(c, ',')) continue;
        if (error) {
            error->message = "Expected ',' or '}'";
            error->offset = c.offset;
        }
        return false;
    }
}

bool skip_value(Cursor& c, ManifestParseError* error) {
    skip_ws(c);
    const char ch = c.peek();
    if (ch == '"') {
        return static_cast<bool>(parse_string(c, error));
    }
    if (ch == '{') {
        return skip_object(c, error);
    }
    if (ch == '[') {
        return skip_array(c, error);
    }
    if (parse_literal(c, "true") || parse_literal(c, "false") || parse_literal(c, "null")) {
        return true;
    }

    // number or unknown token: consume until delimiter.
    if (c.eof()) return false;
    while (!c.eof()) {
        const char t = c.peek();
        if (t == ',' || t == ']' || t == '}' || std::isspace(static_cast<unsigned char>(t))) {
            break;
        }
        c.advance();
    }
    return true;
}

std::optional<std::vector<std::string>> parse_string_array(Cursor& c, ManifestParseError* error) {
    skip_ws(c);
    if (!match(c, '[')) {
        if (error) {
            error->message = "Expected array";
            error->offset = c.offset;
        }
        return std::nullopt;
    }
    skip_ws(c);
    std::vector<std::string> out;
    if (match(c, ']')) return out;

    while (true) {
        auto item = parse_string(c, error);
        if (!item) return std::nullopt;
        out.push_back(std::move(*item));
        skip_ws(c);
        if (match(c, ']')) return out;
        if (match(c, ',')) continue;
        if (error) {
            error->message = "Expected ',' or ']'";
            error->offset = c.offset;
        }
        return std::nullopt;
    }
}

}  // namespace

std::optional<ExtensionManifest> parse_extension_manifest(std::string_view json, ManifestParseError* error) {
    if (error) {
        error->message.clear();
        error->offset = 0;
    }

    Cursor c{json, 0};
    skip_ws(c);
    if (!match(c, '{')) {
        if (error) {
            error->message = "Manifest must be a JSON object";
            error->offset = c.offset;
        }
        return std::nullopt;
    }

    ExtensionManifest out;

    skip_ws(c);
    if (match(c, '}')) {
        if (error) {
            error->message = "Manifest is empty";
            error->offset = c.offset;
        }
        return std::nullopt;
    }

    while (true) {
        auto key = parse_string(c, error);
        if (!key) return std::nullopt;

        if (!match(c, ':')) {
            if (error) {
                error->message = "Expected ':' after object key";
                error->offset = c.offset;
            }
            return std::nullopt;
        }

        if (*key == "name") {
            auto value = parse_string(c, error);
            if (!value) return std::nullopt;
            out.name = std::move(*value);
        } else if (*key == "version") {
            auto value = parse_string(c, error);
            if (!value) return std::nullopt;
            out.version = std::move(*value);
        } else if (*key == "permissions") {
            auto perms = parse_string_array(c, error);
            if (!perms) return std::nullopt;
            out.permissions = std::move(*perms);
        } else if (*key == "declarative_net_request") {
            // MV3's shape: { "rule_resources": ["rules.json"] }. Chrome's real
            // field is an array of objects with id/enabled/path; we take the
            // paths directly, and keep the outer key spelled the same so a
            // manifest written against Chrome's docs reads as expected here.
            skip_ws(c);
            if (!match(c, '{')) {
                if (error) {
                    error->message = "declarative_net_request must be an object";
                    error->offset = c.offset;
                }
                return std::nullopt;
            }
            skip_ws(c);
            if (!match(c, '}')) {
                while (true) {
                    auto dnr_key = parse_string(c, error);
                    if (!dnr_key) return std::nullopt;
                    if (!match(c, ':')) {
                        if (error) {
                            error->message = "Expected ':' after declarative_net_request key";
                            error->offset = c.offset;
                        }
                        return std::nullopt;
                    }
                    if (*dnr_key == "rule_resources") {
                        auto paths = parse_string_array(c, error);
                        if (!paths) return std::nullopt;
                        out.rule_resources = std::move(*paths);
                    } else {
                        if (!skip_value(c, error)) return std::nullopt;
                    }
                    skip_ws(c);
                    if (match(c, '}')) break;
                    if (match(c, ',')) continue;
                    if (error) {
                        error->message = "Expected ',' or '}' in declarative_net_request";
                        error->offset = c.offset;
                    }
                    return std::nullopt;
                }
            }
        } else if (*key == "background") {
            skip_ws(c);
            if (!match(c, '{')) {
                if (error) {
                    error->message = "background must be an object";
                    error->offset = c.offset;
                }
                return std::nullopt;
            }
            skip_ws(c);
            if (!match(c, '}')) {
                while (true) {
                    auto bg_key = parse_string(c, error);
                    if (!bg_key) return std::nullopt;
                    if (!match(c, ':')) {
                        if (error) {
                            error->message = "Expected ':' after background key";
                            error->offset = c.offset;
                        }
                        return std::nullopt;
                    }
                    if (*bg_key == "entry") {
                        auto entry = parse_string(c, error);
                        if (!entry) return std::nullopt;
                        out.background_entry = std::move(*entry);
                    } else {
                        if (!skip_value(c, error)) return std::nullopt;
                    }
                    skip_ws(c);
                    if (match(c, '}')) break;
                    if (match(c, ',')) continue;
                    if (error) {
                        error->message = "Expected ',' or '}' in background";
                        error->offset = c.offset;
                    }
                    return std::nullopt;
                }
            }
        } else {
            if (!skip_value(c, error)) return std::nullopt;
        }

        skip_ws(c);
        if (match(c, '}')) break;
        if (match(c, ',')) continue;
        if (error) {
            error->message = "Expected ',' or '}'";
            error->offset = c.offset;
        }
        return std::nullopt;
    }

    skip_ws(c);
    if (!c.eof()) {
        if (error) {
            error->message = "Unexpected trailing characters";
            error->offset = c.offset;
        }
        return std::nullopt;
    }

    if (out.name.empty()) {
        if (error) error->message = "Missing required field: name";
        return std::nullopt;
    }
    if (out.version.empty()) {
        if (error) error->message = "Missing required field: version";
        return std::nullopt;
    }
    if (out.background_entry.empty()) {
        if (error) error->message = "Missing required field: background.entry";
        return std::nullopt;
    }

    return out;
}

bool manifest_has_permission(const ExtensionManifest& manifest, std::string_view permission) {
    for (const auto& granted : manifest.permissions) {
        if (granted == permission) return true;
    }
    return false;
}

}  // namespace Hummingbird::Engine
