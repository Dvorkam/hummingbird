#pragma once

#include <string>
#include <string_view>

#include "core/dom/Node.h"

namespace Hummingbird::DOM {

class Text : public Node {
public:
    explicit Text(std::string_view text) : m_text(text) {}

    const std::string& get_text() const { return m_text; }
    void append(std::string_view extra) { m_text.append(extra); }

private:
    std::string m_text;
};

}  // namespace Hummingbird::DOM
