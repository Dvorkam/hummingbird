#pragma once

#include <string>
#include <string_view>

#include "core/dom/Node.h"

namespace Hummingbird::DOM {

class Text : public Node {
public:
    static Core::ArenaPtr<Text> create(Core::ArenaAllocator& arena, std::string_view text) {
        return Core::ArenaPtr<Text>(Core::arena_new<Text>(arena, text));
    }

    const std::string& get_text() const { return m_text; }
    void append(std::string_view extra) { m_text.append(extra); }
    void set_text(std::string_view text) { m_text.assign(text); }

private:
    template <typename T, typename... Args>
    // Allow arena_new to invoke the private constructor while keeping creation centralized.
    friend T* Core::arena_new(Core::ArenaAllocator&, Args&&...);

    explicit Text(std::string_view text) : m_text(text) {}

    std::string m_text;
};

}  // namespace Hummingbird::DOM
