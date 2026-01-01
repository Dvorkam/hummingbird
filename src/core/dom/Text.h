#pragma once

#include <string>
#include <string_view>

#include "core/dom/Node.h"

namespace Hummingbird::DOM {

class Text : public Node {
public:
    static ArenaPtr<Text> create(ArenaAllocator& arena, std::string_view text) {
        return ArenaPtr<Text>(arena_new<Text>(arena, text));
    }

    const std::string& get_text() const { return m_text; }
    void append(std::string_view extra) { m_text.append(extra); }

private:
    template <typename T, typename... Args>
    // Allow arena_new to invoke the private constructor while keeping creation centralized.
    friend T* ::arena_new(ArenaAllocator&, Args&&...);

    explicit Text(std::string_view text) : m_text(text) {}

    std::string m_text;
};

}  // namespace Hummingbird::DOM
