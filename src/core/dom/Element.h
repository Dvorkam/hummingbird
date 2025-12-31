#pragma once

#include <string>
#include <string_view>
#include <unordered_map>

#include "core/dom/Node.h"

namespace Hummingbird::DOM {

class Element : public Node {
public:
    static ArenaPtr<Element> create(ArenaAllocator& arena, std::string_view tag_name) {
        void* mem = arena.allocate(sizeof(Element), alignof(Element));
        return ArenaPtr<Element>(new (mem) Element(tag_name));
    }

    const std::string& get_tag_name() const { return m_tag_name; }
    const std::unordered_map<std::string, std::string>& get_attributes() const { return m_attributes; }

    void set_attribute(std::string_view key, std::string_view value) {
        m_attributes[std::string(key)] = std::string(value);
    }

private:
    explicit Element(std::string_view tag_name) : m_tag_name(tag_name) {}

    std::string m_tag_name;
    std::unordered_map<std::string, std::string> m_attributes;
};

}  // namespace Hummingbird::DOM
