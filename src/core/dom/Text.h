#pragma once

#include <string>
#include <string_view>

#include "core/dom/Node.h"

namespace Hummingbird::DOM {

class Text : public Node {
public:
    static ArenaPtr<Text> create(ArenaAllocator& arena, std::string_view text) {
        void* mem = arena.allocate(sizeof(Text), alignof(Text));
        return ArenaPtr<Text>(new (mem) Text(text));
    }

    const std::string& get_text() const { return m_text; }
    void append(std::string_view extra) { m_text.append(extra); }

private:
    explicit Text(std::string_view text) : m_text(text) {}

    std::string m_text;
};

}  // namespace Hummingbird::DOM
