#pragma once

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Hummingbird::Core {

// An ordered list of HTTP header field name/value pairs.
//
// Not a map: HTTP allows a field name to repeat, and `Set-Cookie` in particular
// MUST NOT be collapsed into one comma-joined value (its own grammar uses commas
// in `Expires` dates), so every occurrence is kept separately in receipt order.
// Field names are compared case-insensitively per RFC 9110 and stored as sent.
class HttpHeaders {
public:
    struct Field {
        std::string name;
        std::string value;
    };

    void add(std::string_view name, std::string_view value) {
        fields_.push_back({std::string(name), std::string(value)});
    }

    // Replaces every existing occurrence of `name` with a single field.
    void set(std::string_view name, std::string_view value);

    // Drops every occurrence of `name`. Returns how many were removed.
    size_t remove(std::string_view name);

    // The first value for `name`, or "" when absent. Use for single-valued fields.
    std::string_view get(std::string_view name) const;

    bool contains(std::string_view name) const;

    // Every value for `name`, in receipt order. Use for `Set-Cookie`.
    std::vector<std::string_view> get_all(std::string_view name) const;

    const std::vector<Field>& fields() const { return fields_; }
    bool empty() const { return fields_.empty(); }
    size_t size() const { return fields_.size(); }
    void clear() { fields_.clear(); }

    // Parses one raw "Name: value" line as delivered by a transport. Returns
    // false for a blank line, a status line, or anything without a colon, so
    // callers can feed it every line a backend emits.
    bool add_raw_line(std::string_view line);

private:
    std::vector<Field> fields_;
};

}  // namespace Hummingbird::Core
