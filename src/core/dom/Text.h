#pragma once

#include <string>

#include "core/dom/Node.h"

namespace Hummingbird::DOM {

class Text : public Node {
public:
    Text(const std::string& text) : m_text(text) {}

    const std::string& get_text() const { return m_text; }
    void append(const std::string& extra) { m_text += extra; }

private:
    std::string m_text;
};

}  // namespace Hummingbird::DOM
