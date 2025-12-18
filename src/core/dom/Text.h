#pragma once

#include "core/dom/Node.h"
#include <string>

namespace Hummingbird::DOM {

    class Text : public Node {
    public:
        Text(const std::string& text) : m_text(text) {}

        const std::string& get_text() const { return m_text; }

    private:
        std::string m_text;
    };

}
