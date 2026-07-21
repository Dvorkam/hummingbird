#include "core/net/HttpHeaders.h"

#include <algorithm>

#include "core/utils/StringUtils.h"

namespace Hummingbird::Core {

void HttpHeaders::set(std::string_view name, std::string_view value) {
    remove(name);
    add(name, value);
}

size_t HttpHeaders::remove(std::string_view name) {
    const size_t before = fields_.size();
    fields_.erase(std::remove_if(fields_.begin(), fields_.end(),
                                 [&](const Field& field) { return Utils::equals_ignore_case(field.name, name); }),
                  fields_.end());
    return before - fields_.size();
}

std::string_view HttpHeaders::get(std::string_view name) const {
    for (const auto& field : fields_) {
        if (Utils::equals_ignore_case(field.name, name)) {
            return field.value;
        }
    }
    return {};
}

bool HttpHeaders::contains(std::string_view name) const {
    return std::any_of(fields_.begin(), fields_.end(),
                       [&](const Field& field) { return Utils::equals_ignore_case(field.name, name); });
}

std::vector<std::string_view> HttpHeaders::get_all(std::string_view name) const {
    std::vector<std::string_view> values;
    for (const auto& field : fields_) {
        if (Utils::equals_ignore_case(field.name, name)) {
            values.emplace_back(field.value);
        }
    }
    return values;
}

bool HttpHeaders::add_raw_line(std::string_view line) {
    line = Utils::trim_ascii_whitespace(line);
    if (line.empty()) {
        return false;
    }
    const size_t colon = line.find(':');
    // No colon at all is a status line ("HTTP/1.1 200 OK"); a leading colon is a
    // malformed field with an empty name. Both are skipped rather than stored.
    if (colon == std::string_view::npos || colon == 0) {
        return false;
    }
    const std::string_view name = Utils::trim_ascii_whitespace(line.substr(0, colon));
    const std::string_view value = Utils::trim_ascii_whitespace(line.substr(colon + 1));
    if (name.empty()) {
        return false;
    }
    add(name, value);
    return true;
}

}  // namespace Hummingbird::Core
