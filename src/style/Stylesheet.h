#pragma once

#include <string>
#include <vector>

namespace Hummingbird::Css {

enum class SelectorType { Tag, Class, Id };

struct Selector {
    SelectorType type;
    std::string value;

    int specificity() const {
        switch (type) {
            case SelectorType::Id:
                return 100;
            case SelectorType::Class:
                return 10;
            case SelectorType::Tag:
                return 1;
        }
        return 0;
    }
};

struct Declaration {
    std::string property;
    std::string value;
};

struct Rule {
    std::vector<Selector> selectors;
    std::vector<Declaration> declarations;
};

struct Stylesheet {
    std::vector<Rule> rules;
};

}  // namespace Hummingbird::Css
